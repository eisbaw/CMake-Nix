/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */
#include "cmGlobalNixGenerator.h"

#include <cm/memory>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <set>
#include <functional>
#include <queue>
#include <regex>
#include <cctype>
#include <chrono>
#include <iomanip>
#include <system_error>
#include <exception>
#include <fstream>

#include "cmsys/Directory.hxx"
#include "cmGeneratedFileStream.h"
#include "cmGeneratorTarget.h"
#include "cmLocalNixGenerator.h"
#include "cmMakefile.h"
#include "cmSourceFile.h"
#include "cmSystemTools.h"
#include "cmake.h"
#include "cmNixTargetGenerator.h"
#include "cmNixCustomCommandGenerator.h"
#include "cmNixCustomCommandHandler.h"
#include "cmNixInstallRuleGenerator.h"
#include "cmNixCompilerResolver.h"
#include "cmNixBuildConfiguration.h"
#include "cmNixFileSystemHelper.h"
#include "cmNixPathUtils.h"
#include "cmNixHeaderDependencyResolver.h"
#include "cmNixCacheManager.h"
#include "cmInstallGenerator.h"
#include "cmInstallTargetGenerator.h"
#include "cmCustomCommand.h"
#include "cmListFileCache.h"
#include "cmValue.h"
#include "cmState.h"
#include "cmOutputConverter.h"
#include "cmNixWriter.h"
#include "cmStringAlgorithms.h"
#include "cmNixConstants.h"
#include "cmNixDerivationWriter.h"
#include "cmNixDependencyGraph.h"

// String constants for performance optimization
const std::string cmGlobalNixGenerator::DefaultConfig = "Release";
const std::string cmGlobalNixGenerator::CLanguage = "C";
const std::string cmGlobalNixGenerator::CXXLanguage = "CXX";

cmGlobalNixGenerator::cmGlobalNixGenerator(cmake* cm)
  : cmGlobalCommonGenerator(cm)
  , CompilerResolver(std::make_unique<cmNixCompilerResolver>(cm))
  , DerivationWriter(std::make_unique<cmNixDerivationWriter>())
  , CustomCommandHandler(std::make_unique<cmNixCustomCommandHandler>())
  , InstallRuleGenerator(std::make_unique<cmNixInstallRuleGenerator>())
  , DependencyGraph(std::make_unique<cmNixDependencyGraph>())
  , HeaderDependencyResolver(std::make_unique<cmNixHeaderDependencyResolver>(this))
  , CacheManager(std::make_unique<cmNixCacheManager>())
  , FileSystemHelper(std::make_unique<cmNixFileSystemHelper>(cm))
{
  // Set the make program file
  this->FindMakeProgramFile = "CMakeNixFindMake.cmake";
}

cmGlobalNixGenerator::~cmGlobalNixGenerator() = default;

cmNixCacheManager* cmGlobalNixGenerator::GetCacheManager() const
{
  return this->CacheManager.get();
}

void cmGlobalNixGenerator::LogDebug(const std::string& message) const
{
  if (this->GetCMakeInstance()->GetDebugOutput()) {
    std::ostringstream oss;
    oss << "[NIX-DEBUG] " << message;
    cmSystemTools::Message(oss.str());
  }
}

std::unique_ptr<cmLocalGenerator> cmGlobalNixGenerator::CreateLocalGenerator(
  cmMakefile* mf)
{
  return std::unique_ptr<cmLocalGenerator>(
    cm::make_unique<cmLocalNixGenerator>(this, mf));
}

cmDocumentationEntry cmGlobalNixGenerator::GetDocumentation()
{
  return { cmGlobalNixGenerator::GetActualName(),
           "Generates Nix expressions for building C/C++ projects with "
           "fine-grained derivations for maximal parallelism and caching." };
}

void cmGlobalNixGenerator::Generate()
{
  ProfileTimer timer(this, "cmGlobalNixGenerator::Generate");
  
  this->LogDebug("Generate() started");
  
  // Clear the used derivation names set for fresh generation
  this->CacheManager->ClearUsedDerivationNames();
  
  // Check for unsupported CMAKE_EXPORT_COMPILE_COMMANDS
  if (this->GetCMakeInstance()->GetState()->GetGlobalPropertyAsBool(
        "CMAKE_EXPORT_COMPILE_COMMANDS")) {
    this->GetCMakeInstance()->IssueMessage(
      MessageType::WARNING,
      "CMAKE_EXPORT_COMPILE_COMMANDS is not supported by the Nix generator. "
      "The Nix backend uses derivation-based compilation where commands are "
      "executed inside isolated Nix environments. Consider using Nix-aware "
      "development tools or direnv for IDE integration.");
  }
  
  // Check for ExternalProject_Add or FetchContent usage
  this->CheckForExternalProjectUsage();
  
  // First call the parent Generate to set up targets
  {
    ProfileTimer parentTimer(this, "cmGlobalGenerator::Generate (parent)");
    this->cmGlobalGenerator::Generate();
  }
  
  this->LogDebug("Parent Generate() completed");
  
  // Build dependency graph for transitive dependency resolution
  {
    ProfileTimer graphTimer(this, "BuildDependencyGraph");
    this->BuildDependencyGraph();
  }
  
  // Generate our Nix output
  {
    ProfileTimer writeTimer(this, "WriteNixFile");
    this->WriteNixFile();
  }
  
  this->LogDebug("Generate() completed");
}

std::vector<cmGlobalGenerator::GeneratedMakeCommand>
cmGlobalNixGenerator::GenerateBuildCommand(
  std::string const& makeProgram, std::string const& /*projectName*/,
  std::string const& projectDir,
  std::vector<std::string> const& targetNames, std::string const& /*config*/,
  int /*jobs*/, bool /*verbose*/, cmBuildOptions const& /*buildOptions*/,
  std::vector<std::string> const& /*makeOptions*/)
{
  // Check if this is a try-compile (look for CMakeScratch in path)
  bool isTryCompile = projectDir.find("CMakeScratch") != std::string::npos;
  
  if (this->GetCMakeInstance()->GetDebugOutput()) {
    std::ostringstream msg;
    msg << "GenerateBuildCommand() called for projectDir: " << projectDir
        << " isTryCompile: " << (isTryCompile ? "true" : "false")
        << " targetNames: ";
    for (const auto& t : targetNames) {
      msg << t << " ";
    }
    this->LogDebug(msg.str());
  }
  
  GeneratedMakeCommand makeCommand;
  
  // For Nix generator, we use nix-build as the build program
  makeCommand.Add(this->SelectMakeProgram(makeProgram, cmNix::Commands::NIX_BUILD));
  
  // For try_compile, use the actual project directory
  if (isTryCompile) {
    makeCommand.Add(projectDir + "/" + cmNix::Generator::DEFAULT_NIX);
  } else {
    // Add default.nix file  
    makeCommand.Add(cmNix::Generator::DEFAULT_NIX);
  }
  
  // Add target names as attribute paths  
  for (auto const& tname : targetNames) {
    if (!tname.empty()) {
      makeCommand.Add("-A", tname);
    }
  }
  
  // For try-compile, add post-build copy commands to move binaries from Nix store
  if (isTryCompile && !targetNames.empty()) {
    this->LogDebug(std::string(__FILE__) + ":" + std::to_string(__LINE__) + " Generating try-compile copy commands");
    
    GeneratedMakeCommand copyCommand;
    copyCommand.Add("sh");
    copyCommand.Add("-c");
    
    // Use ostringstream for efficient string concatenation
    std::ostringstream copyScript;
    copyScript << "set -e; ";
    
    for (auto const& tname : targetNames) {
      if (!tname.empty()) {
        this->LogDebug(std::string(__FILE__) + ":" + std::to_string(__LINE__) + " Adding copy command for target: " + tname);
        
        // Read the target location file and copy the binary
        std::string escapedTargetName = cmOutputConverter::EscapeForShell(tname, cmOutputConverter::Shell_Flag_IsUnix);
        std::string locationFile = escapedTargetName + "_loc";
        std::string escapedLocationFile = cmOutputConverter::EscapeForShell(locationFile, cmOutputConverter::Shell_Flag_IsUnix);
        
        copyScript << "if [ -f " << escapedLocationFile << " ]; then ";
        copyScript << "TARGET_LOCATION=$(cat " << escapedLocationFile << "); ";
        if (this->GetCMakeInstance()->GetDebugOutput()) {
          copyScript << "echo '[NIX-DEBUG] Target location: '\"$TARGET_LOCATION\"; ";
        }
        copyScript << "if [ -f \"result\" ]; then ";
        copyScript << "STORE_PATH=$(readlink result); ";
        if (this->GetCMakeInstance()->GetDebugOutput()) {
          copyScript << "echo '[NIX-DEBUG] Store path: '\"$STORE_PATH\"; ";
        }
        copyScript << "cp \"$STORE_PATH\" \"$TARGET_LOCATION\" 2>/dev/null";
        if (this->GetCMakeInstance()->GetDebugOutput()) {
          copyScript << " || echo '[NIX-DEBUG] Copy failed'";
        }
        copyScript << "; ";
        if (this->GetCMakeInstance()->GetDebugOutput()) {
          copyScript << "else echo '[NIX-DEBUG] No result symlink found'; ";
        }
        copyScript << "fi; ";
        if (this->GetCMakeInstance()->GetDebugOutput()) {
          copyScript << "else echo '[NIX-DEBUG] No location file for " 
                     << cmOutputConverter::EscapeForShell(escapedTargetName, cmOutputConverter::Shell_Flag_IsUnix) 
                     << "'; ";
        }
        copyScript << "fi; ";
      }
    }
    copyScript << "true"; // Ensure script always succeeds
    
    copyCommand.Add(copyScript.str());
    
    return { std::move(makeCommand), std::move(copyCommand) };
  }
  
  return { std::move(makeCommand) };
}

void cmGlobalNixGenerator::WriteNixHelperFunctions(cmNixWriter& writer)
{
  // Configure the derivation writer with platform-specific settings
  this->DerivationWriter->SetDebugOutput(this->GetCMakeInstance()->GetDebugOutput());
  this->DerivationWriter->SetObjectFileExtension(this->GetObjectFileExtension());
  this->DerivationWriter->SetSharedLibraryExtension(this->GetSharedLibraryExtension());
  this->DerivationWriter->SetStaticLibraryExtension(this->GetStaticLibraryExtension());
  this->DerivationWriter->SetLibraryPrefix(this->GetLibraryPrefix());
  
  // Write helper functions from the derivation writer
  this->DerivationWriter->WriteNixHelperFunctions(writer);
  
  // Keep the old implementation for now until refactoring is complete
  // Debug: Check if we reach this point
  this->LogDebug("Writing old helper functions after DerivationWriter");
  writer.WriteComment("Helper functions for DRY derivations");
  writer.WriteLine();
  
  // Compilation helper function
  writer.WriteLine("  cmakeNixCC = {");
  writer.WriteLine("    name,");
  writer.WriteLine("    src,");
  writer.WriteLine("    compiler ? gcc,");
  writer.WriteLine("    flags ? \"\",");
  writer.WriteLine("    source,  # Source file path relative to src");
  writer.WriteLine("    buildInputs ? []");
  writer.WriteLine("  }: stdenv.mkDerivation {");
  writer.WriteLine("    inherit name src buildInputs;");
  writer.WriteLine("    dontFixup = true;");
  writer.WriteLine("    buildPhase = ''");
  writer.WriteLine("      mkdir -p \"$(dirname \"$out\")\"");
  writer.WriteLine("      # Store source in a variable to handle paths with spaces");
  writer.WriteLine("      sourceFile=\"${source}\"");
  writer.WriteLine("      # Determine how to invoke the compiler based on the compiler derivation");
  writer.WriteLine("      # When using stdenv.cc, we use the wrapped compiler directly");
  writer.WriteLine("      # For other compilers, we use the binary directly");
  writer.WriteLine("      if [ \"${compiler}\" = \"${stdenv.cc}\" ] || [ \"${compiler}\" = \"${pkgsi686Linux.stdenv.cc}\" ]; then");
  writer.WriteLine("        # stdenv.cc is a wrapped compiler - use it directly");
  writer.WriteLine("        if [[ \"$sourceFile\" == *.cpp ]] || [[ \"$sourceFile\" == *.cxx ]] || [[ \"$sourceFile\" == *.cc ]] || [[ \"$sourceFile\" == *.C ]]; then");
  writer.WriteLine("          compilerCmd=\"${compiler}/bin/g++\"");
  writer.WriteLine("        else");
  writer.WriteLine("          compilerCmd=\"${compiler}/bin/gcc\"");
  writer.WriteLine("        fi");
  writer.WriteLine("      else");
  writer.WriteLine("        # For other compilers, determine the binary name");
  writer.WriteLine("        if [ \"${compiler}\" = \"${gcc}\" ] || [ \"${compiler}\" = \"${pkgsi686Linux.gcc}\" ]; then");
  writer.WriteLine("          if [[ \"$sourceFile\" == *.cpp ]] || [[ \"$sourceFile\" == *.cxx ]] || [[ \"$sourceFile\" == *.cc ]] || [[ \"$sourceFile\" == *.C ]]; then");
  writer.WriteLine("            compilerBin=\"g++\"");
  writer.WriteLine("          else");
  writer.WriteLine("            compilerBin=\"gcc\"");
  writer.WriteLine("          fi");
  writer.WriteLine("        elif [ \"${compiler}\" = \"${clang}\" ] || [ \"${compiler}\" = \"${pkgsi686Linux.clang}\" ]; then");
  writer.WriteLine("          if [[ \"$sourceFile\" == *.cpp ]] || [[ \"$sourceFile\" == *.cxx ]] || [[ \"$sourceFile\" == *.cc ]] || [[ \"$sourceFile\" == *.C ]]; then");
  writer.WriteLine("            compilerBin=\"clang++\"");
  writer.WriteLine("          else");
  writer.WriteLine("            compilerBin=\"clang\"");
  writer.WriteLine("          fi");
  writer.WriteLine("        elif [ \"${compiler}\" = \"${gfortran}\" ] || [ \"${compiler}\" = \"${pkgsi686Linux.gfortran}\" ]; then");
  writer.WriteLine("          compilerBin=\"gfortran\"");
  writer.WriteLine("        else");
  writer.WriteLine("          compilerBin=\"${compiler.pname or \"cc\"}\"");
  writer.WriteLine("        fi");
  writer.WriteLine("        compilerCmd=\"${compiler}/bin/$compilerBin\"");
  writer.WriteLine("      fi");
  writer.WriteLine("      # When src is a directory, Nix unpacks it into a subdirectory");
  writer.WriteLine("      # We need to find the actual source file");
  writer.WriteLine("      # Check if source is an absolute path or Nix expression (e.g., derivation/file)");
  writer.WriteLine("      if [[ \"$sourceFile\" == /* ]] || [[ \"$sourceFile\" == *\"\\$\"* ]]; then");
  writer.WriteLine("        # Absolute path or Nix expression - use as-is");
  writer.WriteLine("        srcFile=\"$sourceFile\"");
  writer.WriteLine("      elif [[ -f \"$sourceFile\" ]]; then");
  writer.WriteLine("        srcFile=\"$sourceFile\"");
  writer.WriteLine("      elif [[ -f \"$(basename \"$src\")/$sourceFile\" ]]; then");
  writer.WriteLine("        srcFile=\"$(basename \"$src\")/$sourceFile\"");
  writer.WriteLine("      else");
  writer.WriteLine("        echo \"Error: Cannot find source file $sourceFile\"");
  writer.WriteLine("        exit 1");
  writer.WriteLine("      fi");
  writer.WriteLine("      $compilerCmd -c ${flags} \"$srcFile\" -o \"$out\"");
  writer.WriteLine("    '';");
  writer.WriteLine("    installPhase = \"true\";");
  writer.WriteLine("  };");
  writer.WriteLine();
  
  // Linking helper function
  // NOTE: This uses Unix-style library naming conventions (lib*.a, lib*.so)
  // This is appropriate since Nix only runs on Unix-like systems (Linux, macOS)
  writer.WriteLine("  # Linking helper function");
  writer.WriteLine("  # NOTE: This uses Unix-style library naming conventions (lib*.a, lib*.so)");
  writer.WriteLine("  # This is appropriate since Nix only runs on Unix-like systems (Linux, macOS)");
  writer.WriteLine("  cmakeNixLD = {");
  writer.WriteLine("    name,");
  writer.WriteLine("    type ? \"executable\",  # \"executable\", \"static\", \"shared\", \"module\"");
  writer.WriteLine("    objects,");
  writer.WriteLine("    compiler ? gcc,");
  writer.WriteLine("    compilerCommand ? null,  # Override compiler binary name (e.g., \"g++\" for C++)");
  writer.WriteLine("    flags ? \"\",");
  writer.WriteLine("    libraries ? [],");
  writer.WriteLine("    buildInputs ? [],");
  writer.WriteLine("    version ? null,");
  writer.WriteLine("    soversion ? null,");
  writer.WriteLine("    postBuildPhase ? \"\"");
  writer.WriteLine("  }: stdenv.mkDerivation {");
  writer.WriteLine("    inherit name objects buildInputs;");
  writer.WriteLine("    dontUnpack = true;");
  writer.WriteLine("    buildPhase =");
  writer.WriteLine("      if type == \"static\" then ''");
  writer.WriteLine("        # Unix static library: uses 'ar' to create lib*.a files");
  writer.WriteLine("        mkdir -p \"$(dirname \"$out\")\"");
  writer.WriteLine("        ar rcs \"$out\" $objects");
  writer.WriteLine("      '' else if type == \"shared\" || type == \"module\" then ''");
  writer.WriteLine("        mkdir -p $out");
  writer.WriteLine("        # Determine compiler command - use stdenv.cc's wrapped compiler when available");
  writer.WriteLine("        if [ \"${compiler}\" = \"${stdenv.cc}\" ] || [ \"${compiler}\" = \"${pkgsi686Linux.stdenv.cc}\" ]; then");
  writer.WriteLine("          # Use compilerCommand override if provided, otherwise use the wrapped compiler");
  writer.WriteLine("          compilerCmd=\"${if compilerCommand != null then compilerCommand else \"${compiler}/bin/gcc\"}\"");
  writer.WriteLine("        else");
  writer.WriteLine("          # For other compilers, use the binary directly");
  writer.WriteLine("          compilerBin=\"${if compilerCommand != null then");
  writer.WriteLine("            compilerCommand");
  writer.WriteLine("          else if compiler == gcc || compiler == pkgsi686Linux.gcc then");
  writer.WriteLine("            \"gcc\"");
  writer.WriteLine("          else if compiler == clang || compiler == pkgsi686Linux.clang then");
  writer.WriteLine("            \"clang\"");
  writer.WriteLine("          else if compiler == gfortran || compiler == pkgsi686Linux.gfortran then");
  writer.WriteLine("            \"gfortran\"");
  writer.WriteLine("          else");
  writer.WriteLine("            compiler.pname or \"cc\"");
  writer.WriteLine("          }\";");
  writer.WriteLine("          compilerCmd=\"${compiler}/bin/$compilerBin\"");
  writer.WriteLine("        fi");
  writer.WriteLine("        # Unix library naming: static=lib*.a, shared=lib*.so, module=*.so");
  writer.WriteLine("        libname=\"${if type == \"module\" then name else \"lib\" + name}.so\"");
  writer.WriteLine("        ${if version != null && type != \"module\" then ''");
  writer.WriteLine("          libname=\"lib${name}.so.${version}\"");
  writer.WriteLine("        '' else \"\"}");
  writer.WriteLine("        $compilerCmd -shared $objects ${flags} ${lib.concatMapStringsSep \" \" (l: l) libraries} -o \"$out/$libname\"");
  writer.WriteLine("        # Create version symlinks if needed (only for shared libraries, not modules)");
  writer.WriteLine("        ${if version != null && type != \"module\" then ''");
  writer.WriteLine("          ln -sf \"$libname\" \"$out/lib${name}.so\"");
  writer.WriteLine("          ${if soversion != null then ''");
  writer.WriteLine("            ln -sf \"$libname\" \"$out/lib${name}.so.${soversion}\"");
  writer.WriteLine("          '' else \"\"}");
  writer.WriteLine("        '' else \"\"}");
  writer.WriteLine("      '' else ''");
  writer.WriteLine("        mkdir -p \"$(dirname \"$out\")\"");
  writer.WriteLine("        # Determine compiler command - use stdenv.cc's wrapped compiler when available");
  writer.WriteLine("        if [ \"${compiler}\" = \"${stdenv.cc}\" ] || [ \"${compiler}\" = \"${pkgsi686Linux.stdenv.cc}\" ]; then");
  writer.WriteLine("          # Use compilerCommand override if provided, otherwise use the wrapped compiler");
  writer.WriteLine("          compilerCmd=\"${if compilerCommand != null then compilerCommand else \"${compiler}/bin/gcc\"}\"");
  writer.WriteLine("        else");
  writer.WriteLine("          # For other compilers, use the binary directly");
  writer.WriteLine("          compilerBin=\"${if compilerCommand != null then");
  writer.WriteLine("            compilerCommand");
  writer.WriteLine("          else if compiler == gcc || compiler == pkgsi686Linux.gcc then");
  writer.WriteLine("            \"gcc\"");
  writer.WriteLine("          else if compiler == clang || compiler == pkgsi686Linux.clang then");
  writer.WriteLine("            \"clang\"");
  writer.WriteLine("          else if compiler == gfortran || compiler == pkgsi686Linux.gfortran then");
  writer.WriteLine("            \"gfortran\"");
  writer.WriteLine("          else");
  writer.WriteLine("            compiler.pname or \"cc\"");
  writer.WriteLine("          }\";");
  writer.WriteLine("          compilerCmd=\"${compiler}/bin/$compilerBin\"");
  writer.WriteLine("        fi");
  writer.WriteLine("        $compilerCmd $objects ${flags} ${lib.concatMapStringsSep \" \" (l: l) libraries} -o \"$out\"");
  writer.WriteLine("      '';");
  writer.WriteLine("    inherit postBuildPhase;");
  writer.WriteLine("    installPhase = \"true\";");
  writer.WriteLine("  };");
  writer.WriteLine();
}

void cmGlobalNixGenerator::WriteNixFile()
{
  ProfileTimer timer(this, "cmGlobalNixGenerator::WriteNixFile");
  
  // Cache frequently used directory path
  const std::string& homeOutputDir = this->GetCMakeInstance()->GetHomeOutputDirectory();
  
  // Write to binary directory to support out-of-source builds
  std::string nixFile = homeOutputDir + "/" + cmNix::Generator::DEFAULT_NIX;
  
  if (this->GetCMakeInstance()->GetDebugOutput()) {
    this->LogDebug("WriteNixFile() writing to: " + nixFile);
  }
  
  cmGeneratedFileStream nixFileStream(nixFile);
  nixFileStream.SetCopyIfDifferent(true);
  
  if (!nixFileStream) {
    std::ostringstream msg;
    msg << "Failed to open Nix file for writing: " << nixFile;
    this->GetCMakeInstance()->IssueMessage(MessageType::FATAL_ERROR, msg.str());
    return;
  }
  
  this->LogDebug("Opened Nix file successfully, starting to write...");

  // Use NixWriter for cleaner code generation
  cmNixWriter writer(nixFileStream);
  
  // Write Nix file header
  writer.WriteComment("Generated by CMake Nix Generator");
  writer.WriteLine(cmNix::Commands::NIXPKGS_IMPORT);
  writer.WriteLine("with pkgs;");
  writer.WriteLine("with lib;");  // Import lib for fileset functions
  writer.WriteLine();
  writer.StartLetBinding();
  
  // Write helper functions for DRY code generation
  {
    ProfileTimer helperTimer(this, "WriteNixHelperFunctions");
    this->WriteNixHelperFunctions(writer);
  }

  // Collect all custom commands using the handler
  std::unordered_map<std::string, cmNixCustomCommandHandler::CustomCommandInfo> collectedCommands;
  {
    ProfileTimer collectTimer(this, "CollectCustomCommands");
    collectedCommands = this->CustomCommandHandler->CollectCustomCommands(this->LocalGenerators);
  }
  
  // Update CustomCommandOutputs map for dependency tracking
  for (const auto& [output, info] : collectedCommands) {
    this->CustomCommandOutputs[output] = info.DerivationName;
    this->LogDebug("Registering custom command output: " + output + " -> " + info.DerivationName);
  }

  // Collect install targets
  this->CollectInstallTargets();

  // Write external header derivations first (before object derivations that depend on them)
  {
    ProfileTimer headerTimer(this, "WriteExternalHeaderDerivations");
    this->HeaderDependencyResolver->WriteExternalHeaderDerivations(nixFileStream);
  }
  
  // Write per-translation-unit derivations BEFORE custom commands
  // so that ObjectFileOutputs is populated when custom commands need it
  {
    ProfileTimer unitTimer(this, "WritePerTranslationUnitDerivations");
    this->WritePerTranslationUnitDerivations(nixFileStream);
  }
  
  // Write custom command derivations AFTER object derivations
  // so that object file dependencies are available
  {
    ProfileTimer customTimer(this, "WriteCustomCommandDerivations");
    this->WriteCustomCommandDerivations(nixFileStream);
  }

  // Write linking derivations
  {
    ProfileTimer linkTimer(this, "WriteLinkingDerivations");
    this->WriteLinkingDerivations(nixFileStream);
  }
  
  // Write install derivations in the let block  
  {
    ProfileTimer installTimer(this, "WriteInstallRules");
    this->WriteInstallRules(nixFileStream);
  }
  
  // End let binding and start attribute set for outputs
  writer.EndLetBinding();
  writer.StartAttributeSet();
  
  // Write final target outputs
  for (auto const& lg : this->LocalGenerators) {
    auto const& targets = lg->GetGeneratorTargets();
    for (auto const& target : targets) {
      if (target->GetType() == cmStateEnums::EXECUTABLE ||
          target->GetType() == cmStateEnums::STATIC_LIBRARY ||
          target->GetType() == cmStateEnums::SHARED_LIBRARY ||
          target->GetType() == cmStateEnums::MODULE_LIBRARY) {
        std::string quotedName = "\"" + target->GetName() + "\"";
        std::string derivation = this->GetDerivationName(target->GetName());
        writer.WriteIndented(1, quotedName + " = " + derivation + ";");
      }
    }
  }

  // Write install outputs
  this->WriteInstallOutputs(nixFileStream);
  
  writer.EndAttributeSet();
}

void cmGlobalNixGenerator::WritePerTranslationUnitDerivations(
  cmGeneratedFileStream& nixFileStream)
{
  cmNixWriter writer(nixFileStream);
  writer.WriteComment("Per-translation-unit derivations");
  
  for (auto const& lg : this->LocalGenerators) {
    auto const& targets = lg->GetGeneratorTargets();
    for (auto const& target : targets) {
      if (target->GetType() == cmStateEnums::EXECUTABLE ||
          target->GetType() == cmStateEnums::STATIC_LIBRARY ||
          target->GetType() == cmStateEnums::SHARED_LIBRARY ||
          target->GetType() == cmStateEnums::MODULE_LIBRARY ||
          target->GetType() == cmStateEnums::OBJECT_LIBRARY) {
        
        // Check for Unity Build and warn if enabled
        if (target->GetPropertyAsBool("UNITY_BUILD")) {
          this->GetCMakeInstance()->IssueMessage(
            MessageType::WARNING,
            cmStrCat("Unity builds are not supported by the Nix generator and will be ignored for target '",
                     target->GetName(), 
                     "'. The Nix backend achieves better performance through fine-grained parallelism."),
            target->GetBacktrace());
        }
        
        // Get source files for this target
        std::vector<cmSourceFile*> sources;
        target->GetSourceFiles(sources, "");
        
        if (this->GetCMakeInstance()->GetDebugOutput()) {
          this->LogDebug("Target " + target->GetName() + " has " + std::to_string(sources.size()) + " source files");
          for (const cmSourceFile* source : sources) {
            this->LogDebug("  Source: " + source->GetFullPath() + 
                          " (Unity: " + (source->GetProperty("UNITY_SOURCE_FILE") ? "yes" : "no") + ")");
          }
        }
        
        // Pre-create target generator and cache configuration for efficiency
        auto targetGen = cmNixTargetGenerator::New(target.get());
        std::string config = this->GetBuildConfiguration(target.get());
        
        // Pre-compute and cache library dependencies for this target
        this->CacheManager->GetLibraryDependencies(target.get(), config,
          [&targetGen, &config]() {
            return targetGen->GetTargetLibraryDependencies(config);
          });
        
        for (const cmSourceFile* source : sources) {
          // Skip Unity-generated batch files (unity_X_cxx.cxx) as we don't support Unity builds
          // But still process the original source files
          std::string sourcePath = source->GetFullPath();
          if (sourcePath.find("/Unity/unity_") != std::string::npos && 
              sourcePath.find("_cxx.cxx") != std::string::npos) {
            this->LogDebug("Skipping Unity batch file: " + sourcePath);
            continue;
          }
          
          std::string const& lang = source->GetLanguage();
          if (lang == "C" || lang == "CXX" || lang == "Fortran" || lang == "CUDA" || 
              lang == "ASM" || lang == "ASM-ATT" || lang == "ASM_NASM" || lang == "ASM_MASM") {
            // Resolve symlinks to ensure the actual file is available in Nix store
            std::string resolvedSourcePath = source->GetFullPath();
            if (cmSystemTools::FileIsSymlink(resolvedSourcePath)) {
              resolvedSourcePath = cmSystemTools::GetRealPath(resolvedSourcePath);
            }
            std::vector<std::string> dependencies = targetGen->GetSourceDependencies(source);
            this->AddObjectDerivation(target->GetName(), this->GetDerivationName(target->GetName(), resolvedSourcePath), resolvedSourcePath, targetGen->GetObjectFileName(source), lang, dependencies);
            this->WriteObjectDerivation(nixFileStream, target.get(), source);
          }
        }
      }
    }
  }
}

void cmGlobalNixGenerator::WriteLinkingDerivations(
  cmGeneratedFileStream& nixFileStream)
{
  nixFileStream << "\n  # Linking derivations\n";
  
  for (auto const& lg : this->LocalGenerators) {
    auto const& targets = lg->GetGeneratorTargets();
    for (auto const& target : targets) {
      if (target->GetType() == cmStateEnums::EXECUTABLE ||
          target->GetType() == cmStateEnums::STATIC_LIBRARY ||
          target->GetType() == cmStateEnums::SHARED_LIBRARY ||
          target->GetType() == cmStateEnums::MODULE_LIBRARY) {
        this->WriteLinkDerivation(nixFileStream, target.get());
      }
    }
  }
}

std::string cmGlobalNixGenerator::GetDerivationName(
  std::string const& targetName, std::string const& sourceFile) const
{
  // Use cache manager to get or compute the derivation name
  return this->CacheManager->GetDerivationName(targetName, sourceFile,
    [this, &targetName, &sourceFile]() {
      std::string result;
  if (sourceFile.empty()) {
    result = "link_" + targetName;
  } else {
    // Use filename with parent directory to make it unique
    std::string filename = cmSystemTools::GetFilenameName(sourceFile);
    std::string parentDir = cmSystemTools::GetFilenameName(
      cmSystemTools::GetFilenamePath(sourceFile));
    
    // Create unique identifier including parent directory
    std::string uniqueName;
    if (!parentDir.empty() && parentDir != ".") {
      uniqueName = parentDir + "_" + filename;
    } else {
      uniqueName = filename;
    }
    
    // Convert to valid Nix identifier
    std::replace(uniqueName.begin(), uniqueName.end(), '.', '_');
    std::replace(uniqueName.begin(), uniqueName.end(), '-', '_');
    result = targetName + "_" + uniqueName + "_o";
  }
  
  // Use the proper function to make a valid Nix identifier
  result = cmNixWriter::MakeValidNixIdentifier(result);
  
  // Ensure uniqueness by checking UsedDerivationNames
  std::string finalResult = result;
  int suffix = 2;
  while (this->CacheManager->IsDerivationNameUsed(finalResult)) {
    finalResult = result + "_" + std::to_string(suffix);
    suffix++;
  }
  this->CacheManager->MarkDerivationNameUsed(finalResult);
  
      return finalResult;
    });
}

void cmGlobalNixGenerator::AddObjectDerivation(std::string const& targetName, std::string const& derivationName, std::string const& sourceFile, std::string const& objectFileName, std::string const& language, std::vector<std::string> const& dependencies)
{
  ObjectDerivation od;
  od.TargetName = targetName;
  od.DerivationName = derivationName;
  od.SourceFile = sourceFile;
  od.ObjectFileName = objectFileName;
  od.Language = language;
  od.Dependencies = dependencies;
  this->ObjectDerivations[derivationName] = od;
  
  // Also track object file path to derivation mapping
  // The object file path might be relative, so we need to handle it properly
  std::string objPath = objectFileName;
  if (!cmSystemTools::FileIsFullPath(objPath)) {
    // Make it relative to the top build directory for consistency
    const std::string& homeOutputDir = this->GetCMakeInstance()->GetHomeOutputDirectory();
    objPath = homeOutputDir + "/" + objPath;
  }
  this->ObjectFileOutputs[objPath] = derivationName;
}

void cmGlobalNixGenerator::WriteObjectDerivation(
  cmGeneratedFileStream& nixFileStream, cmGeneratorTarget* target, 
  const cmSourceFile* source)
{
  // Profile only if CMAKE_NIX_PROFILE_DETAILED=1 to avoid too much output
  const char* detailedProfile = std::getenv("CMAKE_NIX_PROFILE_DETAILED");
  std::unique_ptr<ProfileTimer> timer;
  if (detailedProfile && std::string(detailedProfile) == "1") {
    timer = std::make_unique<ProfileTimer>(this, "WriteObjectDerivation");
  }
  
  // Step 1: Prepare compilation context
  SourceCompilationContext ctx = PrepareSourceCompilationContext(target, source);
  
  this->LogDebug("WriteObjectDerivation for source: " + ctx.sourceFile + 
                " (generated: " + std::to_string(source->GetIsGenerated()) + ")");
  
  // Step 2: Validate source file
  std::string errorMessage;
  if (!this->ValidateSourceFile(source, target, errorMessage)) {
    this->GetCMakeInstance()->IssueMessage(MessageType::WARNING, errorMessage);
    return;
  }
  
  // Emit warning if validation passed but there was a warning message
  if (!errorMessage.empty()) {
    // Only show external source warnings in debug mode or for non-CMake files
    if (this->GetCMakeInstance()->GetDebugOutput() ||
        (errorMessage.find("CMakeC") == std::string::npos &&
         errorMessage.find("CMakeCXX") == std::string::npos)) {
      this->GetCMakeInstance()->IssueMessage(MessageType::WARNING, errorMessage);
    }
  }
  
  // Step 3: Get compile flags
  std::string allCompileFlags = this->GetCompileFlags(target, source, ctx.lang, ctx.config, ctx.objectName);
  
  // Step 4: Process config-time generated files
  ProcessConfigTimeGeneratedFiles(allCompileFlags, ctx.buildDir, ctx.configTimeGeneratedFiles);
  
  // Step 5: Extract include directories from compile flags
  std::vector<std::string> includeDirs;
  std::vector<std::string> parsedIncludeFlags;
  cmSystemTools::ParseUnixCommandLine(allCompileFlags.c_str(), parsedIncludeFlags);
  
  for (const std::string& flag : parsedIncludeFlags) {
    if (flag.substr(0, 2) == "-I") {
      std::string includeDir = flag.substr(2);
      if (!includeDir.empty()) {
        // Ensure absolute path
        if (!cmSystemTools::FileIsFullPath(includeDir)) {
          // Include paths are typically relative to the build directory
          includeDir = ctx.buildDir + "/" + includeDir;
        }
        includeDirs.push_back(includeDir);
      }
    }
  }
  
  // Step 6: Process custom command headers
  ProcessCustomCommandHeaders(ctx.sourceFile, allCompileFlags, includeDirs, ctx.customCommandHeaders);
  
  // Step 7: Initialize DerivationWriter if needed
  if (!this->DerivationWriter) {
    this->DerivationWriter = std::make_unique<cmNixDerivationWriter>();
  }
  this->DerivationWriter->SetDebugOutput(this->GetCMakeInstance()->GetDebugOutput());
  
  // Step 8: Start writing the derivation
  nixFileStream << "  " << ctx.derivName << " = cmakeNixCC {\n";
  nixFileStream << "    name = \"" << ctx.objectName << "\";\n";
  
  // Step 9: Write the src attribute
  WriteSourceAttribute(nixFileStream, ctx, target, source);
  
  // Step 10: Build buildInputs list and write it
  std::string compilerPackage = this->GetCompilerPackage(ctx.lang);
  std::vector<std::string> buildInputs = BuildBuildInputsList(target, source, ctx.config, ctx.sourceFile, ctx.projectSourceRelPath);
  
  if (!buildInputs.empty()) {
    nixFileStream << "    buildInputs = [ ";
    for (size_t i = 0; i < buildInputs.size(); ++i) {
      if (i > 0) nixFileStream << " ";
      nixFileStream << buildInputs[i];
    }
    nixFileStream << " ];\n";
  }
  
  // Step 11: Determine source path
  std::string sourcePath = DetermineSourcePath(ctx.sourceFile, ctx.srcDir, ctx.buildDir);
  
  // Step 12: Update compile flags for generated files
  allCompileFlags = UpdateCompileFlagsForGeneratedFiles(allCompileFlags, ctx.configTimeGeneratedFiles, ctx.buildDir);
  
  // Step 13: Add -fPIC if needed
  std::string allFlags = allCompileFlags;
  if ((target->GetType() == cmStateEnums::SHARED_LIBRARY ||
       target->GetType() == cmStateEnums::MODULE_LIBRARY) &&
      allFlags.find("-fPIC") == std::string::npos) {
    if (!allFlags.empty() && allFlags.back() != ' ') {
      allFlags += " ";
    }
    allFlags += "-fPIC";
  }
  
  // Remove trailing space if any
  if (!allFlags.empty() && allFlags.back() == ' ') {
    allFlags.pop_back();
  }
  
  // Step 14: Write remaining attributes
  if (sourcePath.find("${") != std::string::npos) {
    nixFileStream << "    source = \"" << sourcePath << "\";\n";
  } else {
    nixFileStream << "    source = \"" << cmNixWriter::EscapeNixString(sourcePath) << "\";\n";
  }
  
  WriteCompilerAttribute(nixFileStream, buildInputs, compilerPackage);
  
  if (!allFlags.empty()) {
    nixFileStream << "    flags = \"" << cmNixWriter::EscapeNixString(allFlags) << "\";\n";
  }
  
  // Close the derivation
  nixFileStream << "  };\n\n";
}

void cmGlobalNixGenerator::WriteSourceAttribute(
  cmGeneratedFileStream& nixFileStream,
  const SourceCompilationContext& ctx,
  cmGeneratorTarget* target,
  const cmSourceFile* source)
{
  if (ctx.isExternalSource) {
    WriteExternalSourceComposite(nixFileStream, ctx, target, source);
  } else {
    // Regular project source - always use fileset for better performance
    auto targetGen = cmNixTargetGenerator::New(target);
    std::vector<std::string> dependencies = targetGen->GetSourceDependencies(source);
    
    if (this->GetCMakeInstance()->GetDebugOutput()) {
      this->LogDebug("Source dependencies for " + ctx.sourceFile + ": " + std::to_string(dependencies.size()));
      for (const auto& dep : dependencies) {
        this->LogDebug("  Dependency: " + dep);
      }
    }
    
    // Create lists for existing and generated files
    std::vector<std::string> existingFiles;
    std::vector<std::string> generatedFiles;
    
    // Add the main source file
    std::string relativeSource = cmSystemTools::RelativePath(
      this->GetCMakeInstance()->GetHomeDirectory(), ctx.sourceFile);
    if (!relativeSource.empty() && relativeSource.find("../") != 0) {
      if (source->GetIsGenerated()) {
        generatedFiles.push_back(relativeSource);
      } else {
        existingFiles.push_back(relativeSource);
      }
    }
    
    // Process header dependencies using helper method
    this->LogDebug("Processing headers for " + ctx.sourceFile + ": " + std::to_string(dependencies.size()) + " headers");
    this->HeaderDependencyResolver->ProcessHeaderDependencies(dependencies, ctx.buildDir, ctx.srcDir, 
                                   existingFiles, generatedFiles, const_cast<std::vector<std::string>&>(ctx.configTimeGeneratedFiles));
    
    // Check if we need a composite source
    bool hasExternalIncludes = false;
    cmLocalGenerator* lg = target->GetLocalGenerator();
    std::vector<BT<std::string>> includes = lg->GetIncludeDirectories(target, ctx.lang, ctx.config);
    for (const auto& inc : includes) {
      if (!inc.Value.empty()) {
        std::string incPath = inc.Value;
        if (cmSystemTools::FileIsFullPath(incPath)) {
          std::string relPath = cmSystemTools::RelativePath(ctx.srcDir, incPath);
          if (cmNixPathUtils::IsPathOutsideTree(relPath)) {
            hasExternalIncludes = true;
            break;
          }
        }
      }
    }
    
    // Handle configuration-time generated files or external includes
    if (!ctx.configTimeGeneratedFiles.empty() || hasExternalIncludes || !ctx.customCommandHeaders.empty()) {
      this->WriteCompositeSource(nixFileStream, ctx.configTimeGeneratedFiles, ctx.srcDir, ctx.buildDir, target, ctx.lang, ctx.config, ctx.customCommandHeaders);
    } else if (existingFiles.empty() && generatedFiles.empty()) {
      // No files detected, use whole directory
      nixFileStream << "    src = " << ctx.projectSourceRelPath << ";\n";
    } else {
      // Always use fileset union for minimal source sets to avoid unnecessary rebuilds
      if (!this->UseExplicitSources() && existingFiles.size() + generatedFiles.size() > 0) {
        // When not using explicit sources, only include the source file itself
        existingFiles.clear();
        generatedFiles.clear();
        
        // Re-add just the main source file
        std::string relSource = cmSystemTools::RelativePath(
          this->GetCMakeInstance()->GetHomeDirectory(), ctx.sourceFile);
        if (!relSource.empty() && relSource.find("../") != 0) {
          if (source->GetIsGenerated()) {
            generatedFiles.push_back(relSource);
          } else {
            existingFiles.push_back(relSource);
          }
        }
        
        // Also add include directories and source directory headers
        for (const auto& inc : includes) {
          if (!inc.Value.empty()) {
            std::string incPath = inc.Value;
            // Only add project-relative include directories
            if (!cmSystemTools::FileIsFullPath(incPath)) {
              std::string fullIncPath = this->GetCMakeInstance()->GetHomeDirectory() + "/" + incPath;
              if (cmSystemTools::FileExists(fullIncPath) && cmSystemTools::FileIsDirectory(fullIncPath)) {
                existingFiles.push_back(incPath);
              }
            } else {
              // Check if absolute path is within project
              std::string projectDir = this->GetCMakeInstance()->GetHomeDirectory();
              if (cmSystemTools::IsSubDirectory(incPath, projectDir)) {
                std::string relIncPath = cmSystemTools::RelativePath(projectDir, incPath);
                if (!relIncPath.empty() && relIncPath.find("../") != 0) {
                  existingFiles.push_back(relIncPath);
                }
              }
            }
          }
        }
        
        // Also add the source file's directory headers
        std::string sourceDir = cmSystemTools::GetFilenamePath(relSource);
        if (sourceDir.empty()) {
          sourceDir = ".";
        }
        
        // Check if this directory is already in the fileset
        bool dirAlreadyIncluded = false;
        for (const auto& file : existingFiles) {
          if (file == sourceDir || (sourceDir != "." && file.find(sourceDir + "/") == 0)) {
            dirAlreadyIncluded = true;
            break;
          }
        }
        
        if (!dirAlreadyIncluded) {
          // Add all .h and .hpp files from the source directory
          std::string fullSourceDir;
          if (sourceDir == ".") {
            fullSourceDir = this->GetCMakeInstance()->GetHomeDirectory();
          } else {
            fullSourceDir = this->GetCMakeInstance()->GetHomeDirectory() + "/" + sourceDir;
          }
          
          if (cmSystemTools::FileExists(fullSourceDir) && cmSystemTools::FileIsDirectory(fullSourceDir)) {
            cmsys::Directory dir;
            if (dir.Load(fullSourceDir)) {
              for (unsigned long i = 0; i < dir.GetNumberOfFiles(); ++i) {
                std::string fileName = dir.GetFile(i);
                if (fileName != "." && fileName != "..") {
                  std::string ext = cmSystemTools::GetFilenameLastExtension(fileName);
                  if (ext == ".h" || ext == ".hpp" || ext == ".hxx" || ext == ".H") {
                    if (sourceDir == ".") {
                      existingFiles.push_back(fileName);
                    } else {
                      existingFiles.push_back(sourceDir + "/" + fileName);
                    }
                  }
                }
              }
            }
          }
        }
      }
      
      // Always use fileset union for better caching
      if (existingFiles.size() + generatedFiles.size() > 0) {
        this->WriteFilesetUnion(nixFileStream, existingFiles, generatedFiles, ctx.projectSourceRelPath);
      } else {
        // Fallback to whole directory if no files were collected
        nixFileStream << "    src = " << ctx.projectSourceRelPath << ";\n";
      }
    }
  }
}

void cmGlobalNixGenerator::WriteCompilerAttribute(
  cmGeneratedFileStream& nixFileStream,
  const std::vector<std::string>& buildInputs,
  const std::string& compilerPackage)
{
  std::string finalCompilerPackage = (!buildInputs.empty() ? buildInputs[0] : compilerPackage);
  nixFileStream << "    compiler = " << finalCompilerPackage << ";\n";
}

void cmGlobalNixGenerator::WriteExternalSourceComposite(
  cmGeneratedFileStream& nixFileStream,
  const SourceCompilationContext& ctx,
  cmGeneratorTarget* target,
  const cmSourceFile* source)
{
  // For external sources, create a composite source including both project and external file
  
  // Start composite source derivation
  if (!ctx.configTimeGeneratedFiles.empty()) {
    nixFileStream << "    src = pkgs.runCommand \"composite-src-with-generated\" {\n";
  } else {
    nixFileStream << "    src = pkgs.runCommand \"composite-src\" {\n";
  }
  
  // Add custom command headers as buildInputs
  if (!ctx.customCommandHeaders.empty()) {
    nixFileStream << "      buildInputs = [\n";
    std::set<std::string> processedDerivs;
    for (const auto& headerDeriv : ctx.customCommandHeaders) {
      if (processedDerivs.find(headerDeriv) != processedDerivs.end()) {
        continue;
      }
      processedDerivs.insert(headerDeriv);
      nixFileStream << "        " << headerDeriv << "\n";
    }
    nixFileStream << "      ];\n";
  }
  
  nixFileStream << "    } ''\n";
  nixFileStream << "      mkdir -p $out\n";
  nixFileStream << "      # Copy source files\n";
  nixFileStream << "      cp -rL ${" << ctx.projectSourceRelPath << "}/* $out/ 2>/dev/null || true\n";
  
  // Handle config-time generated files if any
  if (!ctx.configTimeGeneratedFiles.empty()) {
    nixFileStream << "      # Copy configuration-time generated files\n";
    for (const auto& genFile : ctx.configTimeGeneratedFiles) {
      std::string relPath = cmSystemTools::RelativePath(ctx.buildDir, genFile);
      std::string destDir = cmSystemTools::GetFilenamePath(relPath);
      
      // Read the file content and embed it directly
      cmsys::ifstream inFile(genFile.c_str(), std::ios::in | std::ios::binary);
      if (inFile) {
        std::ostringstream contents;
        contents << inFile.rdbuf();
        
        // Create parent directory if needed
        if (!destDir.empty()) {
          nixFileStream << "      mkdir -p $out/" << destDir << "\n";
        }
        
        // Write the file content directly using a here-doc with a unique delimiter
        std::string delimiter = "NIXEOF_" + std::to_string(std::hash<std::string>{}(genFile)) + "_END";
        nixFileStream << "      cat > $out/" << relPath << " <<'" << delimiter << "'\n";
        
        // Escape '' sequences in content since we're inside a Nix multiline string
        std::string contentStr = contents.str();
        for (size_t i = 0; i < contentStr.length(); ++i) {
          if (i + 1 < contentStr.length() && contentStr[i] == '\'' && contentStr[i + 1] == '\'') {
            nixFileStream << "''\\''";
            i++; // Skip the next quote
          } else {
            nixFileStream << contentStr[i];
          }
        }
        
        // Ensure we end with a newline before the delimiter
        if (!contentStr.empty() && contentStr.back() != '\n') {
          nixFileStream << "\n";
        }
        nixFileStream << delimiter << "\n";
      } else {
        // If we can't read the file, issue a warning but continue
        nixFileStream << "      # Warning: Could not read " << genFile << "\n";
      }
    }
  }
  
  // Handle external include directories
  cmLocalGenerator* lg = target->GetLocalGenerator();
  std::vector<BT<std::string>> includes = lg->GetIncludeDirectories(target, ctx.lang, ctx.config);
  
  for (const auto& inc : includes) {
    if (!inc.Value.empty()) {
      std::string incPath = inc.Value;
      
      // Check if this is an absolute path outside the project tree
      if (cmSystemTools::FileIsFullPath(incPath)) {
        std::string relPath = cmSystemTools::RelativePath(ctx.srcDir, incPath);
        if (cmNixPathUtils::IsPathOutsideTree(relPath)) {
          // This is an external include directory
          nixFileStream << "      # Copy headers from external include directory: " << incPath << "\n";
          
          // Normalize the path to resolve any .. segments
          std::string normalizedPath = cmSystemTools::CollapseFullPath(incPath);
          
          // Create parent directories first
          std::string parentPath = cmSystemTools::GetFilenamePath(normalizedPath);
          nixFileStream << "      mkdir -p $out" << parentPath << "\n";
          
          // Use Nix's path functionality to copy the entire directory
          nixFileStream << "      cp -rL ${builtins.path { path = \"" << normalizedPath << "\"; }} $out" << normalizedPath << "\n";
        }
      }
    }
  }
  
  // Copy external source file
  std::string fileName = cmSystemTools::GetFilenameName(ctx.sourceFile);
  nixFileStream << "      # Copy external source file\n";
  nixFileStream << "      cp ${builtins.path { path = \"" << ctx.sourceFile << "\"; }} $out/" << fileName << "\n";
  
  // For ABI detection files, also copy the required header file
  if (fileName.find("CMakeCCompilerABI.c") != std::string::npos ||
      fileName.find("CMakeCXXCompilerABI.cpp") != std::string::npos) {
    std::string abiSourceDir = cmSystemTools::GetFilenamePath(ctx.sourceFile);
    std::string abiHeaderFile = abiSourceDir + "/CMakeCompilerABI.h";
    nixFileStream << "      cp ${builtins.path { path = \"" << abiHeaderFile << "\"; }} $out/CMakeCompilerABI.h\n";
  }
  
  // Handle external headers
  auto targetGen = cmNixTargetGenerator::New(target);
  std::vector<std::string> dependencies = targetGen->GetSourceDependencies(source);
  
  // Collect external headers that need to be made available
  std::vector<std::string> externalHeaders;
  for (const std::string& dep : dependencies) {
    std::string fullPath;
    if (cmSystemTools::FileIsFullPath(dep)) {
      fullPath = dep;
    } else {
      fullPath = this->GetCMakeInstance()->GetHomeDirectory() + "/" + dep;
    }
    
    // Skip if it's a system header or in Nix store
    if (this->IsSystemPath(fullPath)) {
      continue;
    }
    
    // Check if this header is outside the project directory
    std::string relPath = cmSystemTools::RelativePath(
      this->GetCMakeInstance()->GetHomeDirectory(), fullPath);
    if (!relPath.empty() && cmNixPathUtils::IsPathOutsideTree(relPath)) {
      externalHeaders.push_back(fullPath);
    }
  }
  
  // If we have external headers, create or update the header derivation
  if (!externalHeaders.empty()) {
    std::string sourceDir = cmSystemTools::GetFilenamePath(ctx.sourceFile);
    std::string headerDerivName = this->HeaderDependencyResolver->GetOrCreateHeaderDerivation(sourceDir, externalHeaders);
    
    // Store mapping from source file to header derivation
    this->HeaderDependencyResolver->SetSourceHeaderDerivation(ctx.sourceFile, headerDerivName);
    
    // Symlink headers from the header derivation
    nixFileStream << "      # Link headers from external header derivation\n";
    nixFileStream << "      if [ -d ${" << headerDerivName << "} ]; then\n";
    nixFileStream << "        cp -rL ${" << headerDerivName << "}/* $out/ 2>/dev/null || true\n";
    nixFileStream << "      fi\n";
  }
  
  // Copy custom command generated headers
  if (!ctx.customCommandHeaders.empty()) {
    nixFileStream << "      # Copy custom command generated headers\n";
    std::set<std::string> processedDerivs;
    for (const auto& headerDeriv : ctx.customCommandHeaders) {
      if (processedDerivs.find(headerDeriv) != processedDerivs.end()) {
        continue;
      }
      processedDerivs.insert(headerDeriv);
      
      // Find the actual output path for this derivation
      for (const auto& [output, deriv] : this->CustomCommandOutputs) {
        if (deriv == headerDeriv) {
          std::string relativePath = cmSystemTools::RelativePath(ctx.buildDir, output);
          std::string outputDir = cmSystemTools::GetFilenamePath(relativePath);
          if (!outputDir.empty()) {
            nixFileStream << "      mkdir -p $out/" << outputDir << "\n";
          }
          nixFileStream << "      if [ -e ${" << headerDeriv << "}/" << relativePath << " ]; then\n";
          nixFileStream << "        cp ${" << headerDeriv << "}/" << relativePath << " $out/" << relativePath << "\n";
          nixFileStream << "      fi\n";
          break;
        }
      }
    }
  }
  
  nixFileStream << "    '';\n";
}
// [REMOVED DUPLICATE CODE - Lines 1182-1902]
// This was the old WriteObjectDerivation implementation that has been replaced
// by the refactored version using helper methods

bool cmGlobalNixGenerator::ValidateSourceFile(const cmSourceFile* source,
                                               cmGeneratorTarget* target,
                                               std::string& errorMessage)
{
  // Get source file path without modifying any parameters
  const std::string sourceFile = source->GetFullPath();
  
  // Validate source path
  if (sourceFile.empty()) {
    errorMessage = "Empty source file path for target " + target->GetName();
    return false;
  }
  
  // Check if file exists (unless it's a generated file)
  if (!source->GetIsGenerated() && !cmSystemTools::FileExists(sourceFile)) {
    // For generated files, this is expected - warn but continue
    errorMessage = "Source file does not exist: " + sourceFile + " for target " + target->GetName() + 
                   " (might be generated later)";
    // This is a warning, not an error for generated files
    return true;
  }
  
  // Validate path doesn't contain dangerous characters that could break Nix expressions
  if (sourceFile.find('"') != std::string::npos || 
      sourceFile.find('$') != std::string::npos ||
      sourceFile.find('`') != std::string::npos ||
      sourceFile.find('\n') != std::string::npos ||
      sourceFile.find('\r') != std::string::npos) {
    errorMessage = "Source file path contains characters that may break Nix expressions: " + sourceFile;
    // This is a hard error - these characters cannot be safely escaped in all contexts
    return false;
  }
  
  // Additional security check for path traversal
  // CRITICAL FIX: Resolve symlinks BEFORE validation to prevent bypasses
  const std::string normalizedPath = cmSystemTools::CollapseFullPath(sourceFile);
  const std::string resolvedPath = cmSystemTools::GetRealPath(normalizedPath);
  const std::string projectDir = this->GetCMakeInstance()->GetHomeDirectory();
  const std::string resolvedProjectDir = cmSystemTools::GetRealPath(projectDir);
  
  // Check if resolved path is outside project directory (unless it's a system file)
  if (!cmSystemTools::IsSubDirectory(resolvedPath, resolvedProjectDir) &&
      !this->IsSystemPath(resolvedPath)) {
    // Check if it's in the CMake build directory
    const std::string buildDir = this->GetCMakeInstance()->GetHomeOutputDirectory();
    if (!cmSystemTools::IsSubDirectory(normalizedPath, buildDir)) {
      errorMessage = "Source file path is outside project directory: " + sourceFile;
      // This is a warning for CMake internal files (like ABI tests), not an error
      return true;
    }
  }
  
  // All validations passed
  errorMessage.clear();
  return true;
}

std::string cmGlobalNixGenerator::DetermineCompilerPackage(cmGeneratorTarget* target,
                                                           const cmSourceFile* source) const
{
  return this->CompilerResolver->DetermineCompilerPackage(target, source);
}

std::string cmGlobalNixGenerator::GetCompileFlags(cmGeneratorTarget* target,
                                                   const cmSourceFile* source,
                                                   const std::string& lang,
                                                   const std::string& config,
                                                   const std::string& objectName)
{
  cmLocalGenerator* lg = target->GetLocalGenerator();
  
  // Get source and build directories upfront (needed for path processing)
  const std::string& sourceDir = this->GetCMakeInstance()->GetHomeDirectory();
  const std::string& buildDir = this->GetCMakeInstance()->GetHomeOutputDirectory();
  
  // Get configuration-specific compile flags
  std::vector<BT<std::string>> compileFlagsVec = lg->GetTargetCompileFlags(target, config, lang, "");
  std::ostringstream compileFlagsStream;
  bool firstFlag = true;
  
  this->LogDebug("GetCompileFlags called for " + objectName);
  this->LogDebug("Number of compile flags: " + std::to_string(compileFlagsVec.size()));
  
  for (const auto& flag : compileFlagsVec) {
    if (!flag.Value.empty()) {
      std::string trimmedFlag = cmTrimWhitespace(flag.Value);
      
      // Check if the entire string is wrapped in quotes
      if (trimmedFlag.length() >= 2 && 
          trimmedFlag.front() == '"' && trimmedFlag.back() == '"') {
        // Remove the outer quotes
        trimmedFlag = trimmedFlag.substr(1, trimmedFlag.length() - 2);
      }
      
      // Parse the flag string to handle multi-flag strings like "-fPIC -pthread"
      std::vector<std::string> parsedFlags;
      cmSystemTools::ParseUnixCommandLine(trimmedFlag.c_str(), parsedFlags);
      
      // ParseUnixCommandLine should handle all parsing correctly
      // Trust its output and don't second-guess by splitting further
      for (size_t i = 0; i < parsedFlags.size(); ++i) {
        const auto& pFlag = parsedFlags[i];
        
        // Check if this is a flag that takes a file argument
        if ((pFlag == "-imacros" || pFlag == "-include") && i + 1 < parsedFlags.size()) {
          // Handle the flag and its file argument
          if (!firstFlag) {
            compileFlagsStream << " ";
          }
          compileFlagsStream << pFlag;
          firstFlag = false;
          
          // Process the file path argument
          std::string filePath = parsedFlags[++i];
          
          this->LogDebug("Processing " + pFlag + " flag with file: " + filePath);
          this->LogDebug("buildDir: " + buildDir);
          this->LogDebug("sourceDir: " + sourceDir);
          
          // Check if it's an absolute path that needs to be made relative
          if (cmSystemTools::FileIsFullPath(filePath)) {
            // Check if it's in the build directory (configuration-time generated)
            std::string relToBuild = cmSystemTools::RelativePath(buildDir, filePath);
            this->LogDebug("relToBuild: " + relToBuild);
            this->LogDebug("IsPathOutsideTree: " + std::to_string(cmNixPathUtils::IsPathOutsideTree(relToBuild)));
            if (!cmNixPathUtils::IsPathOutsideTree(relToBuild)) {
              // This is a build directory file - for configuration-time generated files
              // that will be embedded, just use the relative path from build dir
              filePath = relToBuild;
              this->LogDebug("Converted to build-relative path: " + filePath);
            } else {
              // Check if it's in the source directory
              std::string relToSource = cmSystemTools::RelativePath(sourceDir, filePath);
              if (!cmNixPathUtils::IsPathOutsideTree(relToSource)) {
                // This is a source directory file - use relative path
                filePath = relToSource;
                this->LogDebug("Converted to source-relative path: " + filePath);
              }
              // Otherwise keep the absolute path (will be handled later)
            }
          }
          
          compileFlagsStream << " " << filePath;
        } else {
          // Regular flag - just add it
          if (!firstFlag) {
            compileFlagsStream << " ";
          }
          compileFlagsStream << pFlag;
          firstFlag = false;
        }
      }
    }
  }
  
  // Get preprocessor definitions
  std::set<BT<std::string>> definesSet = lg->GetTargetDefines(target, config, lang);
  for (const auto& define : definesSet) {
    if (!define.Value.empty()) {
      if (!firstFlag) {
        compileFlagsStream << " ";
      }
      compileFlagsStream << "-D" << define.Value;
      firstFlag = false;
    }
  }
  
  // Get include directories
  std::vector<BT<std::string>> includes = lg->GetIncludeDirectories(target, lang, config);
  
  for (const auto& inc : includes) {
    if (!inc.Value.empty()) {
      std::string incPath = inc.Value;
      
      // Skip system include directories that would be provided by Nix
      if (this->IsSystemPath(incPath)) {
        continue;
      }
      
      // Make include path relative to source directory if possible
      std::string relativeInclude;
      if (cmSystemTools::FileIsFullPath(incPath)) {
        // Normalize the path first to resolve any .. segments
        incPath = cmSystemTools::CollapseFullPath(incPath);
        
        relativeInclude = cmSystemTools::RelativePath(sourceDir, incPath);
        // If the relative path goes outside the source tree, keep absolute
        if (cmNixPathUtils::IsPathOutsideTree(relativeInclude)) {
          relativeInclude = "";
        }
      } else {
        relativeInclude = incPath;
      }
      
      if (!firstFlag) {
        compileFlagsStream << " ";
      }
      std::string finalIncPath = !relativeInclude.empty() ? relativeInclude : incPath;
      // Quote the path if it contains spaces
      if (finalIncPath.find(' ') != std::string::npos) {
        compileFlagsStream << "-I\"" << finalIncPath << "\"";
      } else {
        compileFlagsStream << "-I" << finalIncPath;
      }
      firstFlag = false;
    }
  }
  
  // Add language-specific flags
  if (lang == "CXX") {
    std::string cxxStandard = target->GetFeature("CXX_STANDARD", config);
    if (!cxxStandard.empty()) {
      if (!firstFlag) {
        compileFlagsStream << " ";
      }
      compileFlagsStream << "-std=c++" << cxxStandard;
      firstFlag = false;
    }
  } else if (lang == "C") {
    std::string cStandard = target->GetFeature("C_STANDARD", config);
    if (!cStandard.empty()) {
      if (!firstFlag) {
        compileFlagsStream << " ";
      }
      compileFlagsStream << "-std=c" << cStandard;
      firstFlag = false;
    }
  }
  
  // Add PCH compile options if applicable
  std::vector<std::string> pchArchs = target->GetPchArchs(config, lang);
  std::unordered_set<std::string> pchSources;
  for (const std::string& arch : pchArchs) {
    std::string pchSource = target->GetPchSource(config, lang, arch);
    if (!pchSource.empty()) {
      pchSources.insert(pchSource);
    }
  }
  
  // Check if source file has SKIP_PRECOMPILE_HEADERS property
  std::string sourceFile = source->GetFullPath();
  cmSourceFile* sf = target->Target->GetMakefile()->GetOrCreateSource(sourceFile);
  bool skipPch = sf && sf->GetPropertyAsBool("SKIP_PRECOMPILE_HEADERS");
  
  if (!pchSources.empty() && !skipPch) {
    std::string pchOptions;
    if (pchSources.find(sourceFile) != pchSources.end()) {
      // This is a PCH source file - add create options
      for (const std::string& arch : pchArchs) {
        if (target->GetPchSource(config, lang, arch) == sourceFile) {
          pchOptions = target->GetPchCreateCompileOptions(config, lang, arch);
          break;
        }
      }
    } else {
      // This is a regular source file - add use options
      pchOptions = target->GetPchUseCompileOptions(config, lang);
    }
    
    if (!pchOptions.empty()) {
      // PCH options may be semicolon-separated, convert to space-separated
      std::string processedOptions = pchOptions;
      std::replace(processedOptions.begin(), processedOptions.end(), ';', ' ');
      
      // Convert absolute paths in PCH options to relative paths
      std::string pchProjectDir = this->GetCMakeInstance()->GetHomeDirectory();
      size_t pos = 0;
      while ((pos = processedOptions.find(pchProjectDir, pos)) != std::string::npos) {
        // Find the end of the path (space or end of string)
        size_t endPos = processedOptions.find(' ', pos);
        if (endPos == std::string::npos) {
          endPos = processedOptions.length();
        }
        
        // Extract the full path
        std::string fullPath = processedOptions.substr(pos, endPos - pos);
        
        // Convert to relative path
        std::string relPath = cmSystemTools::RelativePath(pchProjectDir, fullPath);
        
        // Replace in the string
        processedOptions.replace(pos, fullPath.length(), relPath);
        
        // Move past this replacement
        pos += relPath.length();
      }
      
      if (!firstFlag) {
        compileFlagsStream << " ";
      }
      compileFlagsStream << processedOptions;
      firstFlag = false;
    }
  }
  
  // Add output file flag for ASM
  if (lang == "ASM" || lang == "ASM-ATT" || lang == "ASM_NASM" || lang == "ASM_MASM") {
    if (!firstFlag) {
      compileFlagsStream << " ";
    }
    compileFlagsStream << "-o " << objectName;
  }
  
  return compileFlagsStream.str();
}

void cmGlobalNixGenerator::WriteExternalSourceDerivation(cmGeneratedFileStream& /*nixFileStream*/,
                                                         cmGeneratorTarget* /*target*/,
                                                         const cmSourceFile* /*source*/,
                                                         const std::string& /*lang*/,
                                                         const std::string& /*derivName*/,
                                                         const std::string& /*objectName*/)
{
  // External source handling is already implemented in WriteObjectDerivation
  // This is a stub for future refactoring
  // The actual implementation would be moved here from WriteObjectDerivation
}

void cmGlobalNixGenerator::WriteRegularSourceDerivation(cmGeneratedFileStream& /*nixFileStream*/,
                                                        cmGeneratorTarget* /*target*/,
                                                        const cmSourceFile* /*source*/,
                                                        const std::string& /*lang*/,
                                                        const std::string& /*derivName*/,
                                                        const std::string& /*objectName*/)
{
  // Regular source handling is already implemented in WriteObjectDerivation
  // This is a stub for future refactoring
  // The actual implementation would be moved here from WriteObjectDerivation
}


void cmGlobalNixGenerator::WriteLinkDerivation(
  cmGeneratedFileStream& nixFileStream, cmGeneratorTarget* target)
{
  ProfileTimer timer(this, "WriteLinkDerivation");
  
  // Step 1: Prepare link context with all necessary information
  LinkContext ctx = PrepareLinkContext(target);
  
  // Debug output
  const std::string& buildDir = this->GetCMakeInstance()->GetHomeOutputDirectory();
  this->LogDebug("WriteLinkDerivation for target: " + ctx.targetName + 
                " buildDir: " + buildDir + 
                " isTryCompile: " + (ctx.isTryCompile ? "true" : "false"));
  
  // Step 2: Get library dependencies
  std::vector<std::string> libraryDeps = this->GetCachedLibraryDependencies(target, ctx.config);
  
  // Step 3: Collect build inputs
  CollectBuildInputs(ctx, target, libraryDeps);
  
  // Step 4: Collect object files
  CollectObjectFiles(ctx, target);
  
  // Step 5: Process library dependencies
  ProcessLibraryDependencies(ctx, target);
  
  // Step 6: Prepare try_compile post-build phase if needed
  if (ctx.isTryCompile) {
    this->LogDebug("Adding try_compile output file handling for: " + ctx.targetName);
    ctx.postBuildPhase = PrepareTryCompilePostBuildPhase(buildDir, ctx.targetName);
  }
  
  // Step 7: Extract version information
  ExtractVersionInfo(ctx, target);
  
  // Step 8: Initialize DerivationWriter if needed
  if (!this->DerivationWriter) {
    this->DerivationWriter = std::make_unique<cmNixDerivationWriter>();
  }
  
  // Step 9: Delegate to DerivationWriter
  this->DerivationWriter->WriteLinkDerivationWithHelper(
    nixFileStream,
    ctx.derivName,
    ctx.targetName,
    ctx.nixTargetType,
    ctx.buildInputs,
    ctx.objects,
    ctx.compilerPkg,
    ctx.compilerCommand,
    ctx.linkFlagsStr,
    ctx.libraries,
    ctx.versionStr,
    ctx.soversionStr,
    ctx.postBuildPhase
  );
}

std::vector<std::string> cmGlobalNixGenerator::GetSourceDependencies(
  std::string const& /*sourceFile*/) const
{
  // Header dependency tracking is implemented using compiler -MM flags
  return {};
}

std::string cmGlobalNixGenerator::GetCompilerPackage(const std::string& lang) const
{
  std::string result = this->CompilerResolver->GetCompilerPackage(lang);
  
  // Add cross-compilation suffix if needed
  cmake* cm = this->GetCMakeInstance();
  if (cm->GetState()->GetGlobalPropertyAsBool("CMAKE_CROSSCOMPILING")) {
    result += "-cross";
  }
  
  return result;
}

std::string cmGlobalNixGenerator::GetCompilerCommand(const std::string& lang) const
{
  return this->CompilerResolver->GetCompilerCommand(lang);
}

std::string cmGlobalNixGenerator::GetBuildConfiguration(cmGeneratorTarget* target) const
{
  return cmNixBuildConfiguration::GetBuildConfiguration(target, this);
}

std::vector<std::string> cmGlobalNixGenerator::GetCachedLibraryDependencies(
  cmGeneratorTarget* target, const std::string& config) const
{
  // Only profile if CMAKE_NIX_PROFILE_DETAILED=1
  const char* detailedProfile = std::getenv("CMAKE_NIX_PROFILE_DETAILED");
  std::unique_ptr<ProfileTimer> timer;
  if (detailedProfile && std::string(detailedProfile) == "1") {
    timer = std::make_unique<ProfileTimer>(this, "GetCachedLibraryDependencies");
  }
  
  return this->CacheManager->GetLibraryDependencies(target, config,
    [target, &config]() {
      auto targetGen = cmNixTargetGenerator::New(target);
      return targetGen->GetTargetLibraryDependencies(config);
    });
}

void cmGlobalNixGenerator::ProcessLibraryDependenciesForLinking(
  cmGeneratorTarget* target,
  const std::string& config,
  std::vector<std::string>& linkFlagsList,
  std::vector<std::string>& libraries,
  std::set<std::string>& transitiveDeps) const
{
  // Get link implementation
  cmLinkImplementation const* linkImpl = target->GetLinkImplementation(config, cmGeneratorTarget::UseTo::Compile);
  if (!linkImpl) {
    return;
  }
  
  // Get package mapper for imported targets
  auto targetGen = cmNixTargetGenerator::New(target);
  
  // Process each library dependency
  for (const cmLinkItem& item : linkImpl->Libraries) {
    if (item.Target && item.Target->IsImported()) {
      // This is an imported target from find_package
      std::string importedTargetName = item.Target->GetName();
      std::string flags = targetGen->GetPackageMapper().GetLinkFlags(importedTargetName);
      if (!flags.empty()) {
        linkFlagsList.push_back(flags);
      }
    } else if (item.Target && !item.Target->IsImported()) {
      // This is a CMake target within the same project
      std::string depTargetName = item.Target->GetName();
      std::string depDerivName = this->GetDerivationName(depTargetName);
      
      // Add appropriate link flags based on target type using direct references
      if (item.Target->GetType() == cmStateEnums::SHARED_LIBRARY) {
        // For shared libraries, use Nix string interpolation
        libraries.push_back("${" + depDerivName + "}/" + this->GetLibraryPrefix() + 
                           depTargetName + this->GetSharedLibraryExtension());
      } else if (item.Target->GetType() == cmStateEnums::MODULE_LIBRARY) {
        // For module libraries, use Nix string interpolation (no lib prefix)
        libraries.push_back("${" + depDerivName + "}/" + depTargetName + 
                           this->GetSharedLibraryExtension());
      } else if (item.Target->GetType() == cmStateEnums::STATIC_LIBRARY) {
        // For static libraries, link the archive directly using string interpolation
        libraries.push_back("${" + depDerivName + "}");
      }
    } else if (!item.Target) { 
      // External library (not a target)
      std::string libName = item.AsStr();
      linkFlagsList.push_back("-l" + libName);
    }
  }
  
  // Get all transitive shared library dependencies in one call
  // This is more efficient than calling GetTransitiveSharedLibraries for each direct dependency
  transitiveDeps = this->DependencyGraph->GetTransitiveSharedLibraries(target->GetName());
}

void cmGlobalNixGenerator::ProcessLibraryDependenciesForBuildInputs(
  const std::vector<std::string>& libraryDeps,
  std::vector<std::string>& buildInputs,
  const std::string& projectSourceRelPath) const
{
  for (const std::string& lib : libraryDeps) {
    if (!lib.empty()) {
      if (lib.find("__NIXPKG__") == 0) {
        // This is a built-in Nix package
        std::string nixPkg = lib.substr(9); // Remove "__NIXPKG__" prefix
        if (!nixPkg.empty()) {
          // Direct package names from nixpkgs (with pkgs; is at the top)
          // Check if the package name starts with underscore (added by CMake)
          std::string actualPkg = nixPkg;
          if (actualPkg.length() > 1 && actualPkg[0] == '_') {
            // Remove the underscore prefix that CMake adds
            actualPkg = actualPkg.substr(1);
          }
          buildInputs.push_back(actualPkg);
        }
      } else {
        // This is a file import - adjust path for subdirectory sources
        if (!projectSourceRelPath.empty() && lib.find("./") == 0) {
          // Check if the path already has parent directory navigation
          std::string pathAfterDot = lib.substr(2);
          if (cmNixPathUtils::IsPathOutsideTree(pathAfterDot)) {
            // Path already navigates to parent, don't prepend projectSourceRelPath
            buildInputs.push_back("(import " + lib + " { inherit pkgs; })");
          } else {
            // Convert relative path to be relative to project root
            std::string separator = (projectSourceRelPath.empty() || 
                                   projectSourceRelPath.back() == '/') ? "" : "/";
            buildInputs.push_back("(import " + projectSourceRelPath + separator + 
                                 pathAfterDot + " { inherit pkgs; })");
          }
        } else {
          buildInputs.push_back("(import " + lib + " { inherit pkgs; })");
        }
      }
    }
  }
}



void cmGlobalNixGenerator::WriteInstallOutputs(cmGeneratedFileStream& nixFileStream)
{
  std::lock_guard<std::mutex> lock(this->InstallTargetsMutex);
  this->InstallRuleGenerator->WriteInstallOutputs(
    this->InstallTargets, 
    nixFileStream,
    [this](const std::string& targetName) { return this->GetDerivationName(targetName); });
}



void cmGlobalNixGenerator::CollectInstallTargets()
{
  std::lock_guard<std::mutex> lock(this->InstallTargetsMutex);
  this->InstallTargets = this->InstallRuleGenerator->CollectInstallTargets(this->LocalGenerators);
}

void cmGlobalNixGenerator::WriteInstallRules(cmGeneratedFileStream& nixFileStream)
{
  std::lock_guard<std::mutex> lock(this->InstallTargetsMutex);
  std::string config = this->GetBuildConfiguration(nullptr);
  this->InstallRuleGenerator->WriteInstallRules(
    this->InstallTargets, 
    nixFileStream, 
    config,
    [this](const std::string& targetName) { return this->GetDerivationName(targetName); });
}

// Dependency graph implementation
void cmGlobalNixGenerator::BuildDependencyGraph() {
  ProfileTimer timer(this, "BuildDependencyGraph");
  
  // Clear any existing graph
  this->DependencyGraph->Clear();
  
  // Add all targets to the graph
  for (const auto& lg : this->LocalGenerators) {
    for (const auto& target : lg->GetGeneratorTargets()) {
      this->DependencyGraph->AddTarget(target->GetName(), target.get());
    }
  }
  
  // Add dependencies
  std::string config = "Release"; // Default config for dependency analysis
  for (const auto& lg : this->LocalGenerators) {
    for (const auto& target : lg->GetGeneratorTargets()) {
      auto linkImpl = target->GetLinkImplementation(config, cmGeneratorTarget::UseTo::Compile);
      if (linkImpl) {
        for (const cmLinkItem& item : linkImpl->Libraries) {
          if (item.Target && !item.Target->IsImported()) {
            // Add dependency from target to item.Target
            this->DependencyGraph->AddDependency(target->GetName(), item.Target->GetName());
          }
        }
      }
    }
  }
}

bool cmGlobalNixGenerator::UseExplicitSources() const
{
  // Check if CMAKE_NIX_EXPLICIT_SOURCES is set in cache
  cmValue value = this->GetCMakeInstance()->GetState()->GetCacheEntryValue(
    "CMAKE_NIX_EXPLICIT_SOURCES");
  return value && cmIsOn(*value);
}

void cmGlobalNixGenerator::WriteExplicitSourceDerivation(
  cmGeneratedFileStream& nixFileStream,
  const std::string& sourceFile,
  const std::vector<std::string>& dependencies,
  const std::string& projectSourceRelPath)
{
  // Build the list of files to include in the source derivation
  std::set<std::string> filesToInclude;
  
  // Add the source file itself
  filesToInclude.insert(sourceFile);
  
  // Add all dependencies (headers)
  for (const auto& dep : dependencies) {
    filesToInclude.insert(dep);
  }
  
  // Generate a unique name for this source derivation
  std::size_t hash = std::hash<std::string>{}(sourceFile);
  std::stringstream ss;
  ss << std::hex << hash;
  std::string sourceDerivName = "src_" + ss.str().substr(0, 8);
  
  // Write the source derivation using symlinkJoin
  nixFileStream << "    src = stdenv.mkDerivation {\n";
  nixFileStream << "      name = \"" << sourceDerivName << "\";\n";
  nixFileStream << "      dontUnpack = true;\n";
  nixFileStream << "      buildPhase = ''\n";
  nixFileStream << "        mkdir -p $out\n";
  
  // Copy each file to the output, preserving directory structure
  std::string baseDir = this->GetCMakeInstance()->GetHomeDirectory();
  for (const auto& file : filesToInclude) {
    // Make sure the path is absolute
    std::string absPath = file;
    if (!cmSystemTools::FileIsFullPath(absPath)) {
      absPath = baseDir + "/" + file;
    }
    
    // Skip if file doesn't exist (might be system header)
    if (!cmSystemTools::FileExists(absPath)) {
      continue;
    }
    
    std::string relPath = cmSystemTools::RelativePath(baseDir, absPath);
    std::string dirPath = cmSystemTools::GetFilenamePath(relPath);
    
    if (!dirPath.empty()) {
      nixFileStream << "        mkdir -p $out/" << dirPath << "\n";
    }
    nixFileStream << "        cp ${./";
    if (!projectSourceRelPath.empty()) {
      nixFileStream << projectSourceRelPath << "/";
    }
    nixFileStream << relPath << "} $out/" << relPath << "\n";
  }
  
  nixFileStream << "      '';\n";
  nixFileStream << "      installPhase = \"true\";\n";
  nixFileStream << "    };\n";
}


void cmGlobalNixGenerator::WriteCompositeSource(
  cmGeneratedFileStream& nixFileStream,
  const std::vector<std::string>& configTimeGeneratedFiles,
  const std::string& srcDir,
  const std::string& buildDir,
  cmGeneratorTarget* target,
  const std::string& lang,
  const std::string& config,
  const std::vector<std::string>& customCommandHeaders)
{
  // Create a composite source that includes both source files and config-time generated files
  // Build the buildInputs list for custom command dependencies
  nixFileStream << "    src = pkgs.runCommand \"composite-src-with-generated\" {\n";
  if (!customCommandHeaders.empty()) {
    nixFileStream << "      buildInputs = [\n";
    std::set<std::string> processedDerivs;
    for (const auto& headerDeriv : customCommandHeaders) {
      if (processedDerivs.find(headerDeriv) != processedDerivs.end()) {
        continue;
      }
      processedDerivs.insert(headerDeriv);
      nixFileStream << "        " << headerDeriv << "\n";
    }
    nixFileStream << "      ];\n";
  }
  nixFileStream << "    } ''\n";
  nixFileStream << "      mkdir -p $out\n";
  
  // Copy the source directory structure
  nixFileStream << "      # Copy source files\n";
  // For out-of-source builds, compute relative path to source directory
  std::string rootPath = "./.";
  if (srcDir != buildDir) {
    rootPath = cmSystemTools::RelativePath(buildDir, srcDir);
    if (!rootPath.empty()) {
      rootPath = "./" + rootPath;
      // Remove any trailing slash to avoid Nix errors
      if (rootPath.back() == '/') {
        rootPath.pop_back();
      }
    } else {
      rootPath = "./.";
    }
  }
  nixFileStream << "      cp -rL ${" << rootPath << "}/* $out/ 2>/dev/null || true\n";
  
  // Handle external include directories - copy headers from them
  if (target) {
    cmLocalGenerator* lg = target->GetLocalGenerator();
    std::vector<BT<std::string>> includes = lg->GetIncludeDirectories(target, lang, config);
    
    for (const auto& inc : includes) {
      if (!inc.Value.empty()) {
        std::string incPath = inc.Value;
        
        // Check if this is an absolute path outside the project tree
        if (cmSystemTools::FileIsFullPath(incPath)) {
          std::string relPath = cmSystemTools::RelativePath(srcDir, incPath);
          if (cmNixPathUtils::IsPathOutsideTree(relPath)) {
            // This is an external include directory
            nixFileStream << "      # Copy headers from external include directory: " << incPath << "\n";
            
            // Normalize the path to resolve any .. segments
            std::string normalizedPath = cmSystemTools::CollapseFullPath(incPath);
            
            // Create parent directories first
            std::string parentPath = cmSystemTools::GetFilenamePath(normalizedPath);
            nixFileStream << "      mkdir -p $out" << parentPath << "\n";
            
            // Use Nix's path functionality to copy the entire directory
            nixFileStream << "      cp -rL ${builtins.path { path = \"" << normalizedPath << "\"; }} $out" << normalizedPath << "\n";
          }
        }
      }
    }
  }
  
  // Copy configuration-time generated files to their correct locations
  nixFileStream << "      # Copy configuration-time generated files\n";
  
  // Since configuration-time generated files exist in the build directory
  // and Nix can't access them directly with builtins.path (security restriction),
  // we need to embed the file contents directly into the Nix expression
  for (const auto& genFile : configTimeGeneratedFiles) {
    // Calculate the relative path within the build directory
    std::string relPath = cmSystemTools::RelativePath(buildDir, genFile);
    std::string destDir = cmSystemTools::GetFilenamePath(relPath);
    
    // Read the file content and embed it directly
    cmsys::ifstream inFile(genFile.c_str(), std::ios::in | std::ios::binary);
    if (inFile) {
      std::ostringstream contents;
      contents << inFile.rdbuf();
      // File automatically closes when inFile goes out of scope (RAII)
      
      // Create parent directory if needed
      if (!destDir.empty()) {
        nixFileStream << "      mkdir -p $out/" << destDir << "\n";
      }
      
      // Write the file content directly using a here-doc with a unique delimiter
      // Use a complex delimiter to avoid conflicts with file contents
      std::string delimiter = "NIXEOF_" + std::to_string(std::hash<std::string>{}(genFile)) + "_END";
      nixFileStream << "      cat > $out/" << relPath << " <<'" << delimiter << "'\n";
      
      // Escape '' sequences in content since we're inside a Nix multiline string
      std::string contentStr = contents.str();
      for (size_t i = 0; i < contentStr.length(); ++i) {
        if (i + 1 < contentStr.length() && contentStr[i] == '\'' && contentStr[i + 1] == '\'') {
          nixFileStream << "''\\''";
          i++; // Skip the next quote
        } else {
          nixFileStream << contentStr[i];
        }
      }
      
      // Ensure we end with a newline before the delimiter
      if (!contentStr.empty() && contentStr.back() != '\n') {
        nixFileStream << "\n";
      }
      nixFileStream << delimiter << "\n";
    } else {
      // If we can't read the file, issue a warning but continue
      std::ostringstream msg;
      msg << "Warning: Cannot read configuration-time generated file: " << genFile;
      this->GetCMakeInstance()->IssueMessage(MessageType::WARNING, msg.str());
      nixFileStream << "      # Warning: Could not read " << genFile << "\n";
    }
  }
  
  // Copy custom command generated headers
  if (!customCommandHeaders.empty()) {
    nixFileStream << "      # Copy custom command generated headers\n";
    // Use a set to track unique derivation names to avoid duplicates
    std::set<std::string> processedDerivs;
    for (const auto& headerDeriv : customCommandHeaders) {
      if (processedDerivs.find(headerDeriv) != processedDerivs.end()) {
        continue; // Already processed this derivation
      }
      processedDerivs.insert(headerDeriv);
      
      // Find ALL header outputs for this derivation
      for (const auto& [output, deriv] : this->CustomCommandOutputs) {
        if (deriv == headerDeriv) {
          // Check if this output is a header file
          std::string ext = cmSystemTools::GetFilenameLastExtension(output);
          if (ext == ".h" || ext == ".hpp" || ext == ".hxx" || ext == ".H") {
            std::string relPath = cmSystemTools::RelativePath(buildDir, output);
            std::string destDir = cmSystemTools::GetFilenamePath(relPath);
            if (!destDir.empty()) {
              nixFileStream << "      mkdir -p $out/" << destDir << "\n";
            }
            nixFileStream << "      cp $" << headerDeriv << "/" << relPath << " $out/" << relPath << "\n";
          }
        }
      }
    }
  }
  
  nixFileStream << "    '';\n";
}

void cmGlobalNixGenerator::WriteFilesetUnion(
  cmGeneratedFileStream& nixFileStream,
  const std::vector<std::string>& existingFiles,
  const std::vector<std::string>& generatedFiles,
  const std::string& rootPath)
{
  // Write fileset union
  nixFileStream << "    src = fileset.toSource {\n";
  nixFileStream << "      root = " << rootPath << ";\n";
  nixFileStream << "      fileset = fileset.unions [\n";
  
  // Add existing files
  for (const auto& file : existingFiles) {
    // Check if path contains spaces, special characters, or non-ASCII characters that need quoting
    bool needsQuoting = false;
    for (char c : file) {
      if (c == ' ' || c == '\'' || c == '"' || c == '$' || c == '\\' || 
          static_cast<unsigned char>(c) > 127) {
        needsQuoting = true;
        break;
      }
    }
    
    if (needsQuoting) {
      // Use string concatenation syntax for paths with special characters
      nixFileStream << "        (" << rootPath << " + \"/" << cmNixWriter::EscapeNixString(file) << "\")\n";
    } else {
      nixFileStream << "        " << rootPath << "/" << file << "\n";
    }
  }
  
  // Add generated files with maybeMissing
  for (const auto& file : generatedFiles) {
    // Check if path contains spaces, special characters, or non-ASCII characters that need quoting
    bool needsQuoting = false;
    for (char c : file) {
      if (c == ' ' || c == '\'' || c == '"' || c == '$' || c == '\\' || 
          static_cast<unsigned char>(c) > 127) {
        needsQuoting = true;
        break;
      }
    }
    
    if (needsQuoting) {
      // Use string concatenation syntax for paths with special characters
      nixFileStream << "        (fileset.maybeMissing (" << rootPath << " + \"/" << cmNixWriter::EscapeNixString(file) << "\"))\n";
    } else {
      nixFileStream << "        (fileset.maybeMissing " << rootPath << "/" << file << ")\n";
    }
  }
  
  nixFileStream << "      ];\n";
  nixFileStream << "    };\n";
}

std::vector<std::string> cmGlobalNixGenerator::BuildBuildInputsList(
  cmGeneratorTarget* target,
  const cmSourceFile* source,
  const std::string& config,
  const std::string& sourceFile,
  const std::string& projectSourceRelPath)
{
  std::vector<std::string> buildInputs;
  
  // Add compiler package
  std::string lang = source->GetLanguage();
  std::string compilerPkg = this->DetermineCompilerPackage(target, source);
  
  // Check if we need to use 32-bit compiler
  cmLocalGenerator* lg = target->GetLocalGenerator();
  std::vector<BT<std::string>> compileFlagsVec = lg->GetTargetCompileFlags(target, config, lang, "");
  bool needs32Bit = false;
  for (const auto& flag : compileFlagsVec) {
    if (flag.Value.find("-m32") != std::string::npos) {
      needs32Bit = true;
      break;
    }
  }
  
  // For C++ code, we should use the wrapped stdenv.cc instead of raw gcc
  if (lang == "CXX") {
    if (needs32Bit) {
      compilerPkg = "pkgsi686Linux.stdenv.cc";
    } else {
      compilerPkg = "stdenv.cc";
    }
  } else {
    // Use the regular compiler packages for non-C++ code
    if (needs32Bit && compilerPkg == "gcc") {
      compilerPkg = "pkgsi686Linux.gcc";
    } else if (needs32Bit && compilerPkg == "clang") {
      compilerPkg = "pkgsi686Linux.clang";
    }
  }
  
  buildInputs.push_back(compilerPkg);
  
  this->LogDebug("Language: " + lang + 
                ", Compiler package: " + compilerPkg + 
                (needs32Bit ? " (32-bit)" : ""));
  
  // Get external library dependencies
  std::vector<std::string> libraryDeps = this->GetCachedLibraryDependencies(target, config);
  this->ProcessLibraryDependenciesForBuildInputs(libraryDeps, buildInputs, projectSourceRelPath);
  
  // Check if this source file is generated by a custom command
  auto it = this->CustomCommandOutputs.find(sourceFile);
  if (it != this->CustomCommandOutputs.end()) {
    buildInputs.push_back(it->second);
    this->LogDebug("Found custom command dependency for " + sourceFile + " -> " + it->second);
  } else {
    if (this->GetCMakeInstance()->GetDebugOutput()) {
      this->LogDebug("No custom command found for " + sourceFile);
      this->LogDebug("Available custom command outputs:");
      for (const auto& kv : this->CustomCommandOutputs) {
        this->LogDebug("  " + kv.first + " -> " + kv.second);
      }
    }
  }
  
  // Check if any header dependencies are generated by custom commands
  auto targetGen = cmNixTargetGenerator::New(target);
  std::vector<std::string> headers = targetGen->GetSourceDependencies(source);
  
  if (!headers.empty()) {
    this->LogDebug("Checking header dependencies for " + sourceFile);
    for (const auto& header : headers) {
      this->LogDebug("  Header: " + header);
    }
  }
  
  for (const auto& header : headers) {
    // Check multiple possible paths for the header
    std::vector<std::string> pathsToCheck;
    
    if (cmSystemTools::FileIsFullPath(header)) {
      pathsToCheck.push_back(header);
    } else {
      // Try with source directory
      pathsToCheck.push_back(this->GetCMakeInstance()->GetHomeDirectory() + "/" + header);
      // Try with binary directory
      const std::string& homeOutputDir = this->GetCMakeInstance()->GetHomeOutputDirectory();
      pathsToCheck.push_back(homeOutputDir + "/" + header);
      // Try with the header as-is (relative path)
      pathsToCheck.push_back(header);
    }
    
    
    // Check each possible path
    bool found = false;
    for (const auto& pathToCheck : pathsToCheck) {
      auto customIt = this->CustomCommandOutputs.find(pathToCheck);
      if (customIt != this->CustomCommandOutputs.end()) {
        // Only add if not already in buildInputs
        if (std::find(buildInputs.begin(), buildInputs.end(), customIt->second) == buildInputs.end()) {
          buildInputs.push_back(customIt->second);
          this->LogDebug("Found custom command generated header dependency: " + 
                        header + " (resolved to " + pathToCheck + ") -> " + customIt->second);
          found = true;
          break;
        }
      }
    }
    
    if (!found) {
      this->LogDebug("Header " + header + " not found in custom command outputs");
      this->LogDebug("Checked paths:");
      for (const auto& path : pathsToCheck) {
        this->LogDebug("  - " + path);
      }
    }
  }
  
  // Check if this source file has an external header derivation dependency
  std::string headerDerivation = this->HeaderDependencyResolver->GetSourceHeaderDerivation(sourceFile);
  if (!headerDerivation.empty()) {
    buildInputs.push_back(headerDerivation);
    this->LogDebug("Found header derivation dependency for " + sourceFile + " -> " + headerDerivation);
  }
  
  return buildInputs;
}


bool cmGlobalNixGenerator::IsSystemPath(const std::string& path) const
{
  return this->FileSystemHelper->IsSystemPath(path);
}

void cmGlobalNixGenerator::CheckForExternalProjectUsage()
{
  // Check all makefiles in the project for ExternalProject or FetchContent usage
  bool hasExternalProject = false;
  bool hasFetchContent = false;
  std::set<std::string> externalProjectTargets;
  std::set<std::string> fetchContentDependencies;
  
  for (const auto& lg : this->LocalGenerators) {
    cmMakefile* mf = lg->GetMakefile();
    
    // Check for commands that indicate module usage
    // We'll check the list files for include() statements instead
    
    // Also check for include() commands in listfiles
    const auto& listFiles = mf->GetListFiles();
    for (const auto& file : listFiles) {
      std::ifstream infile(file);
      std::string line;
      while (std::getline(infile, line)) {
        // Simple pattern matching for include() commands
        if (line.find("include(ExternalProject)") != std::string::npos ||
            line.find("include( ExternalProject )") != std::string::npos) {
          hasExternalProject = true;
        }
        if (line.find("include(FetchContent)") != std::string::npos ||
            line.find("include( FetchContent )") != std::string::npos) {
          hasFetchContent = true;
        }
      }
    }
  }
  
  // Issue warnings if these modules are used
  if (hasExternalProject) {
    this->GetCMakeInstance()->IssueMessage(
      MessageType::WARNING,
      "ExternalProject_Add is incompatible with the Nix generator.\n"
      "ExternalProject downloads dependencies at build time, which conflicts "
      "with Nix's pure build philosophy.\n\n"
      "Recommended alternatives:\n"
      "  1. Pre-fetch dependencies and add to Nix store\n"
      "  2. Use find_package() with Nix-provided packages\n"
      "  3. Include dependencies as Git submodules\n"
      "  4. Create pkg_<Package>.nix files for external dependencies\n\n"
      "The Nix generator will create a default.nix file, but builds may fail "
      "when ExternalProject tries to download content.");
  }
  
  if (hasFetchContent) {
    this->GetCMakeInstance()->IssueMessage(
      MessageType::WARNING,
      "FetchContent is incompatible with the Nix generator.\n"
      "FetchContent downloads dependencies at configure time, which conflicts "
      "with Nix's pure build philosophy.\n\n"
      "Recommended alternatives:\n"
      "  1. Pre-fetch dependencies and add to Nix store\n"
      "  2. Use find_package() with Nix-provided packages\n"
      "  3. Include dependencies as Git submodules\n"
      "  4. Create pkg_<Package>.nix files for external dependencies\n\n"
      "Example: For FetchContent_Declare(fmt ...), create pkg_fmt.nix:\n"
      "  { fmt }:\n"
      "  {\n"
      "    buildInputs = [ fmt ];\n"
      "    cmakeFlags = [];\n"
      "  }");
  }
  
  // Additionally, we could generate skeleton pkg_*.nix files for known dependencies
  if (hasExternalProject || hasFetchContent) {
    this->GenerateSkeletonPackageFiles();
  }
}

void cmGlobalNixGenerator::WriteCustomCommandDerivations(
  cmGeneratedFileStream& nixFileStream)
{
  // Collect custom commands if not already done
  auto collectedCommands = this->CustomCommandHandler->CollectCustomCommands(this->LocalGenerators);
  
  // Delegate to handler
  this->CustomCommandHandler->WriteCustomCommandDerivations(
    collectedCommands, 
    &this->CustomCommandOutputs, 
    &this->ObjectFileOutputs,
    nixFileStream,
    this->GetCMakeInstance()->GetHomeDirectory(),
    this->GetCMakeInstance()->GetHomeOutputDirectory(),
    this->GetCMakeInstance()->GetDebugOutput());
}

void cmGlobalNixGenerator::GenerateSkeletonPackageFiles()
{
  // This method will scan for common external dependencies and generate
  // skeleton pkg_*.nix files that users can fill in
  
  // For now, let's check for some common packages
  std::map<std::string, std::string> commonPackages = {
    {"fmt", "{ fmt }:\n{\n  buildInputs = [ fmt ];\n  cmakeFlags = [];\n}"},
    {"json", "{ nlohmann_json }:\n{\n  buildInputs = [ nlohmann_json ];\n  cmakeFlags = [];\n}"},
    {"googletest", "{ gtest }:\n{\n  buildInputs = [ gtest ];\n  cmakeFlags = [];\n}"},
    {"boost", "{ boost }:\n{\n  buildInputs = [ boost ];\n  cmakeFlags = [];\n}"}
  };

  // Check if any of these packages are referenced in the project
  for (const auto& lg : this->LocalGenerators) {
    cmMakefile* mf = lg->GetMakefile();
    const auto& listFiles = mf->GetListFiles();
    
    for (const auto& file : listFiles) {
      std::ifstream infile(file);
      std::string line;
      while (std::getline(infile, line)) {
        // Look for FetchContent_Declare or ExternalProject_Add
        for (const auto& pkg : commonPackages) {
          if (line.find(pkg.first) != std::string::npos &&
              (line.find("FetchContent_Declare") != std::string::npos ||
               line.find("ExternalProject_Add") != std::string::npos)) {
            // Generate skeleton file if it doesn't exist
            const std::string& homeOutputDir = this->GetCMakeInstance()->GetHomeOutputDirectory();
            std::string pkgFileName = homeOutputDir + 
                                    "/pkg_" + pkg.first + ".nix";
            
            this->LogDebug("Found " + pkg.first + " in line: " + line);
            this->LogDebug("Would create: " + pkgFileName);
            
            if (!cmSystemTools::FileExists(pkgFileName)) {
              {
                std::ofstream pkgFile(pkgFileName);
                pkgFile << "# Skeleton Nix package file for " << pkg.first << "\n";
                pkgFile << "# Edit this file to specify the correct Nix package\n";
                pkgFile << pkg.second << "\n";
                // File automatically closes when pkgFile goes out of scope (RAII)
              }
              
              this->GetCMakeInstance()->IssueMessage(
                MessageType::AUTHOR_WARNING,
                "Generated skeleton pkg_" + pkg.first + ".nix file. "
                "Please edit it to specify the correct Nix package.");
            }
          }
        }
      }
    }
  }
}

// Profiling support implementation
bool cmGlobalNixGenerator::GetProfilingEnabled() const
{
  // Check for CMAKE_NIX_PROFILE environment variable
  const char* profile = std::getenv("CMAKE_NIX_PROFILE");
  return profile != nullptr && std::string(profile) == "1";
}

cmGlobalNixGenerator::ProfileTimer::ProfileTimer(
  const cmGlobalNixGenerator* gen, const std::string& name)
  : Generator(gen), Name(name)
{
  if (Generator->GetProfilingEnabled()) {
    StartTime = std::chrono::steady_clock::now();
    std::cerr << "[NIX-PROFILE] START: " << Name << std::endl;
  }
}

cmGlobalNixGenerator::ProfileTimer::~ProfileTimer()
{
  if (Generator->GetProfilingEnabled()) {
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
      endTime - StartTime);
    double ms = duration.count() / 1000.0;
    std::cerr << "[NIX-PROFILE] END: " << Name 
              << " (duration: " << std::fixed << std::setprecision(3) 
              << ms << " ms)" << std::endl;
  }
}

// Helper method implementations for WriteObjectDerivation refactoring

cmGlobalNixGenerator::SourceCompilationContext 
cmGlobalNixGenerator::PrepareSourceCompilationContext(
  cmGeneratorTarget* target,
  const cmSourceFile* source)
{
  SourceCompilationContext ctx;
  
  // Get source file and resolve symlinks
  ctx.sourceFile = source->GetFullPath();
  if (cmSystemTools::FileIsSymlink(ctx.sourceFile)) {
    ctx.sourceFile = cmSystemTools::GetRealPath(ctx.sourceFile);
  }
  
  // Get derivation name and object info
  ctx.derivName = this->GetDerivationName(target->GetName(), ctx.sourceFile);
  const ObjectDerivation& od = this->ObjectDerivations[ctx.derivName];
  ctx.objectName = od.ObjectFileName;
  ctx.lang = od.Language;
  ctx.headers = od.Dependencies;
  
  // Get configuration
  ctx.config = target->Target->GetMakefile()->GetSafeDefinition("CMAKE_BUILD_TYPE");
  if (ctx.config.empty()) {
    ctx.config = "Release"; // Default configuration
  }
  
  // Get directory paths - cache to avoid repeated calls
  const std::string& homeOutputDir = this->GetCMakeInstance()->GetHomeOutputDirectory();
  const std::string& homeDir = this->GetCMakeInstance()->GetHomeDirectory();
  ctx.buildDir = homeOutputDir;
  ctx.srcDir = homeDir;
  
  // Calculate project source relative path
  ctx.projectSourceRelPath = "./.";
  if (ctx.srcDir != ctx.buildDir) {
    ctx.projectSourceRelPath = cmSystemTools::RelativePath(ctx.buildDir, ctx.srcDir);
    if (!ctx.projectSourceRelPath.empty()) {
      ctx.projectSourceRelPath = "./" + ctx.projectSourceRelPath;
      // Remove any trailing slash to avoid Nix errors
      if (ctx.projectSourceRelPath.back() == '/') {
        ctx.projectSourceRelPath.pop_back();
      }
    } else {
      ctx.projectSourceRelPath = "./.";
    }
  }
  
  // Check if source is external
  std::string initialRelativePath = cmSystemTools::RelativePath(
    this->GetCMakeInstance()->GetHomeDirectory(), ctx.sourceFile);
  ctx.isExternalSource = (cmNixPathUtils::IsPathOutsideTree(initialRelativePath) || 
                         cmSystemTools::FileIsFullPath(initialRelativePath));
  
  return ctx;
}

void cmGlobalNixGenerator::ProcessConfigTimeGeneratedFiles(
  const std::string& allCompileFlags,
  const std::string& buildDir,
  std::vector<std::string>& configTimeGeneratedFiles)
{
  // Process files referenced by -imacros and -include flags
  std::vector<std::string> parsedFlags;
  cmSystemTools::ParseUnixCommandLine(allCompileFlags.c_str(), parsedFlags);
  
  for (size_t i = 0; i < parsedFlags.size(); ++i) {
    const auto& flag = parsedFlags[i];
    if ((flag == "-imacros" || flag == "-include") && i + 1 < parsedFlags.size()) {
      std::string filePath = parsedFlags[++i];
      
      // Convert relative path to absolute if needed
      if (!cmSystemTools::FileIsFullPath(filePath)) {
        filePath = buildDir + "/" + filePath;
      }
      
      // Check if it's a build directory file (configuration-time generated)
      std::string relToBuild = cmSystemTools::RelativePath(buildDir, filePath);
      if (!cmNixPathUtils::IsPathOutsideTree(relToBuild) && cmSystemTools::FileExists(filePath)) {
        // This is a configuration-time generated file that needs to be embedded
        configTimeGeneratedFiles.push_back(filePath);
        this->LogDebug("Added " + flag + " file to config-time generated: " + filePath);
      }
    }
  }
}

void cmGlobalNixGenerator::ProcessCustomCommandHeaders(
  const std::string& sourceFile,
  const std::string& allCompileFlags,
  const std::vector<std::string>& includeDirs,
  std::vector<std::string>& customCommandHeaders)
{
  // Check all custom command outputs to see if they're in any include directories
  for (const auto& [output, deriv] : this->CustomCommandOutputs) {
    std::string outputDir = cmSystemTools::GetFilenamePath(output);
    
    // Check if this output is in any of our include directories
    for (const std::string& includeDir : includeDirs) {
      // Resolve both paths to handle relative paths correctly
      std::string fullOutputDir = cmSystemTools::CollapseFullPath(outputDir);
      std::string fullIncludeDir = cmSystemTools::CollapseFullPath(includeDir);
      
      if (outputDir == fullIncludeDir || 
          cmSystemTools::IsSubDirectory(output, fullIncludeDir)) {
        
        // This header is in an include directory, add it as a dependency
        if (std::find(customCommandHeaders.begin(), customCommandHeaders.end(), deriv) == customCommandHeaders.end()) {
          customCommandHeaders.push_back(deriv);
          this->LogDebug("Found custom command header in include dir: " + output + " -> " + deriv);
        }
        break;
      }
    }
  }
  
  // Also check for headers that might be included via relative paths
  std::ifstream sourceStream(sourceFile);
  if (sourceStream) {
    std::string line;
    std::regex includeRegex(R"(^\s*#\s*include\s*["<]([^">]+)[">])");
    while (std::getline(sourceStream, line)) {
      std::smatch match;
      if (std::regex_match(line, match, includeRegex)) {
        std::string includedFile = match[1];
        
        // Build list of paths to check
        std::vector<std::string> pathsToCheck;
        
        // For relative includes, check relative to source file directory
        if (!cmSystemTools::FileIsFullPath(includedFile)) {
          std::string sourceDir = cmSystemTools::GetFilenamePath(sourceFile);
          pathsToCheck.push_back(sourceDir + "/" + includedFile);
        }
        
        // Also check in all include directories
        for (const std::string& includeDir : includeDirs) {
          pathsToCheck.push_back(includeDir + "/" + includedFile);
        }
        
        // Make absolute paths
        for (size_t i = 0; i < pathsToCheck.size(); ++i) {
          if (!cmSystemTools::FileIsFullPath(pathsToCheck[i])) {
            pathsToCheck[i] = cmSystemTools::CollapseFullPath(pathsToCheck[i]);
          }
        }
        
        // Check each possible path
        for (const auto& pathToCheck : pathsToCheck) {
          auto customIt = this->CustomCommandOutputs.find(pathToCheck);
          if (customIt != this->CustomCommandOutputs.end()) {
            customCommandHeaders.push_back(customIt->second);
            this->LogDebug("Found custom command header for composite source: " + pathToCheck + " -> " + customIt->second);
            break;
          }
        }
      }
    }
  }
}

std::string cmGlobalNixGenerator::DetermineSourcePath(
  const std::string& sourceFile,
  const std::string& projectSourceDir,
  const std::string& projectBuildDir)
{
  std::string sourcePath;
  std::string customCommandDep;
  
  auto customIt = this->CustomCommandOutputs.find(sourceFile);
  if (customIt != this->CustomCommandOutputs.end()) {
    customCommandDep = customIt->second;
  }
  
  if (!customCommandDep.empty()) {
    // Source is generated by a custom command - reference from derivation output
    const std::string& topBuildDir = this->GetCMakeInstance()->GetHomeOutputDirectory();
    std::string relativePath = cmSystemTools::RelativePath(topBuildDir, sourceFile);
    sourcePath = "${" + customCommandDep + "}/" + relativePath;
  } else {
    // All files (source and generated) - use relative path from source directory
    std::string sourceFileRelativePath = cmSystemTools::RelativePath(projectSourceDir, sourceFile);
    
    // Check if this is an external file (outside project tree)
    if (cmNixPathUtils::IsPathOutsideTree(sourceFileRelativePath) || cmSystemTools::FileIsFullPath(sourceFileRelativePath)) {
      // External file - use just the filename, it will be copied to source dir
      std::string fileName = cmSystemTools::GetFilenameName(sourceFile);
      sourcePath = fileName;
    } else {
      // File within project tree (source or generated)
      // Check if file is in build directory for out-of-source builds
      if (projectSourceDir != projectBuildDir && sourceFile.find(projectBuildDir) == 0) {
        // File is in build directory - calculate path relative to build dir
        std::string buildRelativePath = cmSystemTools::RelativePath(projectBuildDir, sourceFile);
        
        // For out-of-source builds, prefix with build directory relative path
        std::string srcToBuildRelPath = cmSystemTools::RelativePath(projectSourceDir, projectBuildDir);
        if (!srcToBuildRelPath.empty()) {
          sourcePath = srcToBuildRelPath + "/" + buildRelativePath;
        } else {
          sourcePath = buildRelativePath;
        }
      } else {
        // File is in source directory
        sourcePath = sourceFileRelativePath;
      }
    }
  }
  
  return sourcePath;
}

std::string cmGlobalNixGenerator::UpdateCompileFlagsForGeneratedFiles(
  std::string allCompileFlags,
  const std::vector<std::string>& configTimeGeneratedFiles,
  const std::string& buildDir)
{
  // Update compile flags to use relative paths for embedded config-time generated files
  for (const auto& genFile : configTimeGeneratedFiles) {
    std::string absPath = genFile;
    std::string relPath = cmSystemTools::RelativePath(buildDir, genFile);
    
    // Replace absolute path with relative path in compile flags
    size_t pos = 0;
    while ((pos = allCompileFlags.find(absPath, pos)) != std::string::npos) {
      allCompileFlags.replace(pos, absPath.length(), relPath);
      pos += relPath.length();
    }
    
    this->LogDebug("Replaced " + absPath + " with " + relPath + " in compile flags");
  }
  
  return allCompileFlags;
}

// WriteLinkDerivation helper methods

cmGlobalNixGenerator::LinkContext 
cmGlobalNixGenerator::PrepareLinkContext(cmGeneratorTarget* target)
{
  LinkContext ctx;
  
  ctx.targetName = target->GetName();
  ctx.derivName = this->GetDerivationName(ctx.targetName);
  
  // Determine paths - cache to avoid repeated calls
  const std::string& sourceDir = this->GetCMakeInstance()->GetHomeDirectory();
  const std::string& buildDir = this->GetCMakeInstance()->GetHomeOutputDirectory();
  ctx.projectSourceRelPath = cmSystemTools::RelativePath(buildDir, sourceDir);
  
  // Check if this is a try_compile
  ctx.isTryCompile = buildDir.find("CMakeScratch") != std::string::npos;
  
  // Get configuration
  ctx.config = this->GetBuildConfiguration(target);
  
  // Determine output name and type
  ctx.outputName = this->DetermineOutputName(target);
  ctx.nixTargetType = this->MapTargetTypeToNix(target);
  
  // Determine primary language
  ctx.primaryLang = this->DeterminePrimaryLanguage(target);
  ctx.compilerPkg = this->GetCompilerPackage(ctx.primaryLang);
  ctx.compilerCommand = this->GetCompilerCommand(ctx.primaryLang);
  
  return ctx;
}

std::string cmGlobalNixGenerator::DeterminePrimaryLanguage(cmGeneratorTarget* target)
{
  std::string primaryLang = "C";
  std::vector<cmSourceFile*> sources;
  target->GetSourceFiles(sources, "");
  
  for (cmSourceFile* source : sources) {
    std::string lang = source->GetLanguage();
    if (lang == "CXX") {
      primaryLang = "CXX";
      break;  // C++ takes precedence
    } else if (lang == "Fortran" && primaryLang == "C") {
      primaryLang = "Fortran";  // Fortran takes precedence over C
    }
  }
  
  return primaryLang;
}

std::string cmGlobalNixGenerator::DetermineOutputName(cmGeneratorTarget* target)
{
  std::string targetName = target->GetName();
  
  if (target->GetType() == cmStateEnums::SHARED_LIBRARY) {
    return this->GetLibraryPrefix() + targetName + this->GetSharedLibraryExtension();
  } else if (target->GetType() == cmStateEnums::MODULE_LIBRARY) {
    return targetName + this->GetSharedLibraryExtension();  // Modules typically don't have lib prefix
  } else {
    return targetName;
  }
}

std::string cmGlobalNixGenerator::MapTargetTypeToNix(cmGeneratorTarget* target)
{
  switch (target->GetType()) {
    case cmStateEnums::STATIC_LIBRARY:
      return "static";
    case cmStateEnums::SHARED_LIBRARY:
      return "shared";
    case cmStateEnums::MODULE_LIBRARY:
      return "module";
    default:
      return "executable";
  }
}

void cmGlobalNixGenerator::CollectBuildInputs(
  LinkContext& ctx, 
  cmGeneratorTarget* target,
  const std::vector<std::string>& libraryDeps)
{
  // Start with compiler package
  ctx.buildInputs.push_back(ctx.compilerPkg);
  
  // Add external library dependencies
  this->ProcessLibraryDependenciesForBuildInputs(libraryDeps, ctx.buildInputs, ctx.projectSourceRelPath);
  
  // Get transitive shared library dependencies
  std::set<std::string> transitiveDeps = this->DependencyGraph->GetTransitiveSharedLibraries(ctx.targetName);
  std::set<std::string> directSharedDeps;
  
  // Get link implementation
  auto linkImpl = target->GetLinkImplementation(ctx.config, cmGeneratorTarget::UseTo::Compile);
  
  // Add direct CMake target dependencies (only shared libraries)
  if (linkImpl) {
    for (const cmLinkItem& item : linkImpl->Libraries) {
      if (item.Target && !item.Target->IsImported()) {
        // Only add shared and module libraries to buildInputs, not static libraries
        if (item.Target->GetType() == cmStateEnums::SHARED_LIBRARY ||
            item.Target->GetType() == cmStateEnums::MODULE_LIBRARY) {
          std::string depTargetName = item.Target->GetName();
          std::string depDerivName = this->GetDerivationName(depTargetName);
          ctx.buildInputs.push_back(depDerivName);
          directSharedDeps.insert(depTargetName);
        }
      }
    }
  }
  
  // Add transitive shared library dependencies (excluding direct ones)
  for (const std::string& depTarget : transitiveDeps) {
    if (directSharedDeps.find(depTarget) == directSharedDeps.end()) {
      std::string depDerivName = this->GetDerivationName(depTarget);
      ctx.buildInputs.push_back(depDerivName);
    }
  }
}

void cmGlobalNixGenerator::CollectObjectFiles(
  LinkContext& ctx,
  cmGeneratorTarget* target)
{
  std::vector<cmSourceFile*> sources;
  target->GetSourceFiles(sources, "");
  
  // Get PCH sources to exclude from linking
  std::unordered_set<std::string> pchSources;
  std::set<std::string> languages;
  target->GetLanguages(languages, ctx.config);
  
  for (const std::string& lang : languages) {
    std::vector<std::string> pchArchs = target->GetPchArchs(ctx.config, lang);
    for (const std::string& arch : pchArchs) {
      std::string pchSource = target->GetPchSource(ctx.config, lang, arch);
      if (!pchSource.empty()) {
        pchSources.insert(pchSource);
      }
    }
  }
  
  // Process regular source files
  for (cmSourceFile* source : sources) {
    // Skip Unity-generated batch files
    std::string sourcePath = source->GetFullPath();
    if (sourcePath.find("/Unity/unity_") != std::string::npos && 
        sourcePath.find("_cxx.cxx") != std::string::npos) {
      continue;
    }
    
    std::string const& lang = source->GetLanguage();
    if (lang == "C" || lang == "CXX" || lang == "Fortran" || lang == "CUDA" || 
        lang == "ASM" || lang == "ASM-ATT" || lang == "ASM_NASM" || lang == "ASM_MASM") {
      // Exclude PCH source files from linking
      std::string resolvedSourcePath = source->GetFullPath();
      if (cmSystemTools::FileIsSymlink(resolvedSourcePath)) {
        resolvedSourcePath = cmSystemTools::GetRealPath(resolvedSourcePath);
      }
      if (pchSources.find(resolvedSourcePath) == pchSources.end()) {
        std::string objDerivName = this->GetDerivationName(
          target->GetName(), resolvedSourcePath);
        ctx.objects.push_back(objDerivName);
      }
    }
  }
  
  // Add object files from OBJECT libraries
  std::vector<cmSourceFile const*> externalObjects;
  target->GetExternalObjects(externalObjects, ctx.config);
  
  for (cmSourceFile const* source : externalObjects) {
    std::string objectFile = source->GetFullPath();
    
    // Remove .o extension to get the source file path
    std::string sourceFile = objectFile;
    std::string objExt = this->GetObjectFileExtension();
    if (sourceFile.size() > objExt.size() && 
        sourceFile.substr(sourceFile.size() - objExt.size()) == objExt) {
      sourceFile = sourceFile.substr(0, sourceFile.size() - objExt.size());
    }
    
    // Find the OBJECT library that contains this source
    for (auto const& lg : this->LocalGenerators) {
      auto const& targets = lg->GetGeneratorTargets();
      for (auto const& objTarget : targets) {
        if (objTarget->GetType() == cmStateEnums::OBJECT_LIBRARY) {
          std::vector<cmSourceFile*> objSources;
          objTarget->GetSourceFiles(objSources, ctx.config);
          for (cmSourceFile* objSource : objSources) {
            if (objSource->GetFullPath() == sourceFile) {
              // Found the OBJECT library that contains this source
              std::string objDerivName = this->GetDerivationName(
                objTarget->GetName(), sourceFile);
              ctx.objects.push_back(objDerivName);
              goto next_external_object;
            }
          }
        }
      }
    }
    next_external_object:;
  }
}

void cmGlobalNixGenerator::ProcessLibraryDependencies(
  LinkContext& ctx,
  cmGeneratorTarget* target)
{
  // Get library dependencies
  std::set<std::string> transitiveDeps = this->DependencyGraph->GetTransitiveSharedLibraries(ctx.targetName);
  
  // Get link implementation
  auto linkImpl = target->GetLinkImplementation(ctx.config, cmGeneratorTarget::UseTo::Compile);
  
  // Check if target depends on any static libraries
  bool hasStaticDependencies = false;
  std::set<std::string> directStaticLibs;
  
  if (linkImpl) {
    for (const cmLinkItem& item : linkImpl->Libraries) {
      if (item.Target && !item.Target->IsImported() &&
          item.Target->GetType() == cmStateEnums::STATIC_LIBRARY) {
        hasStaticDependencies = true;
        directStaticLibs.insert(item.Target->GetName());
      }
    }
  }
  
  // Process library dependencies
  std::vector<std::string> linkFlagsList;
  
  if (!hasStaticDependencies) {
    // No static dependencies - process normally
    this->ProcessLibraryDependenciesForLinking(target, ctx.config, linkFlagsList, ctx.libraries, transitiveDeps);
  } else {
    // Has static dependencies - handle specially
    if (linkImpl) {
      auto targetGen = cmNixTargetGenerator::New(target);
      for (const cmLinkItem& item : linkImpl->Libraries) {
        if (item.Target && item.Target->IsImported()) {
          // Imported target
          std::string importedTargetName = item.Target->GetName();
          std::string flags = targetGen->GetPackageMapper().GetLinkFlags(importedTargetName);
          if (!flags.empty()) {
            linkFlagsList.push_back(flags);
          }
        } else if (item.Target && !item.Target->IsImported()) {
          // CMake target within the same project
          if (item.Target->GetType() != cmStateEnums::STATIC_LIBRARY) {
            // Add non-static libraries normally
            std::string depTargetName = item.Target->GetName();
            std::string depDerivName = this->GetDerivationName(depTargetName);
            if (item.Target->GetType() == cmStateEnums::SHARED_LIBRARY) {
              ctx.libraries.push_back("${" + depDerivName + "}/" + this->GetLibraryPrefix() + 
                                     depTargetName + this->GetSharedLibraryExtension());
            } else if (item.Target->GetType() == cmStateEnums::MODULE_LIBRARY) {
              ctx.libraries.push_back("${" + depDerivName + "}/" + depTargetName + 
                                     this->GetSharedLibraryExtension());
            }
          }
        } else if (!item.Target) {
          // External library
          std::string libName = item.AsStr();
          linkFlagsList.push_back("-l" + libName);
        }
      }
    }
    
    // Handle static library dependencies
    this->HandleStaticLibraryDependencies(ctx, target, directStaticLibs, transitiveDeps);
  }
  
  // Convert linkFlags to string
  if (!linkFlagsList.empty()) {
    ctx.linkFlagsStr = cmJoin(linkFlagsList, " ");
  }
}

void cmGlobalNixGenerator::HandleStaticLibraryDependencies(
  LinkContext& ctx,
  cmGeneratorTarget* target,
  const std::set<std::string>& directStaticLibs,
  const std::set<std::string>& transitiveDeps)
{
  // Get all targets in topological order for proper static library linking
  std::vector<std::string> topologicalOrder = this->DependencyGraph->GetTopologicalOrderForLinking(target->GetName());
  
  // Also need to include direct static dependencies
  std::set<std::string> allStaticDeps = this->DependencyGraph->GetAllTransitiveDependencies(target->GetName());
  allStaticDeps.insert(directStaticLibs.begin(), directStaticLibs.end());
  
  if (this->GetCMakeInstance()->GetDebugOutput()) {
    this->LogDebug("Topological order for linking " + target->GetName() + ":");
    for (const auto& t : topologicalOrder) {
      this->LogDebug("  " + t);
    }
  }
  
  // Track what's already added
  std::set<std::string> alreadyAdded;
  for (const std::string& lib : ctx.libraries) {
    // Extract target name from library reference
    size_t start = lib.find("${link_") + 7;
    size_t end = lib.find("}");
    if (start != std::string::npos && end != std::string::npos && start < end) {
      alreadyAdded.insert(lib.substr(start, end - start));
    }
  }
  
  // Add both direct and transitive static libraries in topological order
  for (const std::string& depTarget : topologicalOrder) {
    if (allStaticDeps.count(depTarget) > 0 && alreadyAdded.find(depTarget) == alreadyAdded.end()) {
      auto depIt = std::find_if(this->LocalGenerators.begin(), this->LocalGenerators.end(),
        [&depTarget](const std::unique_ptr<cmLocalGenerator>& lg) {
          auto const& targets = lg->GetGeneratorTargets();
          return std::any_of(targets.begin(), targets.end(),
            [&depTarget](const std::unique_ptr<cmGeneratorTarget>& t) {
              return t->GetName() == depTarget;
            });
        });
      
      if (depIt != this->LocalGenerators.end()) {
        auto const& targets = (*depIt)->GetGeneratorTargets();
        auto targetIt = std::find_if(targets.begin(), targets.end(),
          [&depTarget](const std::unique_ptr<cmGeneratorTarget>& t) {
            return t->GetName() == depTarget;
          });
        
        if (targetIt != targets.end()) {
          std::string depDerivName = this->GetDerivationName(depTarget);
          if ((*targetIt)->GetType() == cmStateEnums::STATIC_LIBRARY) {
            ctx.libraries.push_back("${" + depDerivName + "}");
          } else if ((*targetIt)->GetType() == cmStateEnums::SHARED_LIBRARY) {
            ctx.libraries.push_back("${" + depDerivName + "}/" + this->GetLibraryPrefix() + 
                                   depTarget + this->GetSharedLibraryExtension());
          } else if ((*targetIt)->GetType() == cmStateEnums::MODULE_LIBRARY) {
            ctx.libraries.push_back("${" + depDerivName + "}/" + depTarget + 
                                   this->GetSharedLibraryExtension());
          }
        }
      }
    }
  }
  
  // For static libraries, reverse the order
  if (!ctx.libraries.empty()) {
    std::reverse(ctx.libraries.begin(), ctx.libraries.end());
  }
  
  // Add transitive shared library dependencies for targets without static dependencies
  std::set<std::string> directSharedDeps;
  for (const std::string& depTarget : transitiveDeps) {
    if (directSharedDeps.find(depTarget) == directSharedDeps.end()) {
      std::string depDerivName = this->GetDerivationName(depTarget);
      ctx.libraries.push_back("${" + depDerivName + "}/" + this->GetLibraryPrefix() + 
                             depTarget + this->GetSharedLibraryExtension());
    }
  }
}

std::string cmGlobalNixGenerator::PrepareTryCompilePostBuildPhase(
  const std::string& buildDir,
  const std::string& targetName)
{
  std::ostringstream postBuildStream;
  postBuildStream << "      # Create output location in build directory for CMake COPY_FILE\n";
  std::string escapedBuildDir = cmOutputConverter::EscapeForShell(buildDir, cmOutputConverter::Shell_Flag_IsUnix);
  std::string escapedTargetName = cmOutputConverter::EscapeForShell(targetName, cmOutputConverter::Shell_Flag_IsUnix);
  postBuildStream << "      COPY_DEST=" << escapedBuildDir << "/" << escapedTargetName << "\n";
  postBuildStream << "      cp \"$out\" \"$COPY_DEST\"\n";
  
  if (this->GetCMakeInstance()->GetDebugOutput()) {
    postBuildStream << "      echo '[NIX-DEBUG] Copied try_compile output to: '\"$COPY_DEST\"\n";
  }
  
  postBuildStream << "      # Write location file that CMake expects to find the executable path\n";
  postBuildStream << "      echo \"$COPY_DEST\" > " << escapedBuildDir << "/" << escapedTargetName << "_loc\n";
  
  if (this->GetCMakeInstance()->GetDebugOutput()) {
    postBuildStream << "      echo '[NIX-DEBUG] Wrote location file: '" << escapedBuildDir << "/" << escapedTargetName << "_loc\n";
    postBuildStream << "      echo '[NIX-DEBUG] Location file contains: '\"$COPY_DEST\"\n";
  }
  
  return postBuildStream.str();
}

void cmGlobalNixGenerator::ExtractVersionInfo(
  LinkContext& ctx,
  cmGeneratorTarget* target)
{
  if (target->GetType() == cmStateEnums::SHARED_LIBRARY) {
    cmValue version = target->GetProperty("VERSION");
    cmValue soversion = target->GetProperty("SOVERSION");
    if (version) {
      ctx.versionStr = *version;
    }
    if (soversion) {
      ctx.soversionStr = *soversion;
    }
  }
}
