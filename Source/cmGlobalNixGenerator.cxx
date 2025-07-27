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
#include "cmNixCompilerResolver.h"
#include "cmNixPathUtils.h"
#include "cmInstallGenerator.h"
#include "cmInstallTargetGenerator.h"
#include "cmCustomCommand.h"
#include "cmListFileCache.h"
#include "cmValue.h"
#include "cmState.h"
#include "cmOutputConverter.h"
#include "cmNixWriter.h"
#include "cmStringAlgorithms.h"

// String constants for performance optimization
const std::string cmGlobalNixGenerator::DefaultConfig = "Release";
const std::string cmGlobalNixGenerator::CLanguage = "C";
const std::string cmGlobalNixGenerator::CXXLanguage = "CXX";

cmGlobalNixGenerator::cmGlobalNixGenerator(cmake* cm)
  : cmGlobalCommonGenerator(cm)
  , CompilerResolver(std::make_unique<cmNixCompilerResolver>(cm))
{
  // Set the make program file
  this->FindMakeProgramFile = "CMakeNixFindMake.cmake";
}

cmGlobalNixGenerator::~cmGlobalNixGenerator() = default;

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
  
  if (this->GetCMakeInstance()->GetDebugOutput()) {
    std::cerr << "[NIX-DEBUG] " << __FILE__ << ":" << __LINE__ << " Generate() started" << std::endl;
  }
  
  // Clear the used derivation names set for fresh generation
  {
    std::lock_guard<std::mutex> lock(this->UsedNamesMutex);
    this->UsedDerivationNames.clear();
  }
  
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
  
  if (this->GetCMakeInstance()->GetDebugOutput()) {
    std::cerr << "[NIX-DEBUG] " << __FILE__ << ":" << __LINE__ << " Parent Generate() completed" << std::endl;
  }
  
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
  
  if (this->GetCMakeInstance()->GetDebugOutput()) {
    std::cerr << "[NIX-DEBUG] " << __FILE__ << ":" << __LINE__ << " Generate() completed" << std::endl;
  }
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
    std::cerr << "[NIX-DEBUG] " << __FILE__ << ":" << __LINE__ 
              << " GenerateBuildCommand() called for projectDir: " << projectDir
              << " isTryCompile: " << (isTryCompile ? "true" : "false")
              << " targetNames: ";
    for (const auto& t : targetNames) {
      std::cerr << t << " ";
    }
    std::cerr << std::endl;
  }
  
  GeneratedMakeCommand makeCommand;
  
  // For Nix generator, we use nix-build as the build program
  makeCommand.Add(this->SelectMakeProgram(makeProgram, "nix-build"));
  
  // For try_compile, use the actual project directory
  if (isTryCompile) {
    makeCommand.Add(projectDir + "/default.nix");
  } else {
    // Add default.nix file  
    makeCommand.Add("default.nix");
  }
  
  // Add target names as attribute paths  
  for (auto const& tname : targetNames) {
    if (!tname.empty()) {
      makeCommand.Add("-A", tname);
    }
  }
  
  // For try-compile, add post-build copy commands to move binaries from Nix store
  if (isTryCompile && !targetNames.empty()) {
    if (this->GetCMakeInstance()->GetDebugOutput()) {
      std::cerr << "[NIX-DEBUG] " << __FILE__ << ":" << __LINE__ 
                << " Generating try-compile copy commands" << std::endl;
    }
    
    GeneratedMakeCommand copyCommand;
    copyCommand.Add("sh");
    copyCommand.Add("-c");
    
    // Use ostringstream for efficient string concatenation
    std::ostringstream copyScript;
    copyScript << "set -e; ";
    
    for (auto const& tname : targetNames) {
      if (!tname.empty()) {
        if (this->GetCMakeInstance()->GetDebugOutput()) {
          std::cerr << "[NIX-DEBUG] " << __FILE__ << ":" << __LINE__ 
                    << " Adding copy command for target: " << tname << std::endl;
        }
        
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
  
  // Write to binary directory to support out-of-source builds
  std::string nixFile = this->GetCMakeInstance()->GetHomeOutputDirectory();
  nixFile += "/default.nix";
  
  if (this->GetCMakeInstance()->GetDebugOutput()) {
    std::cerr << "[NIX-DEBUG] WriteNixFile() writing to: " << nixFile << std::endl;
  }
  
  cmGeneratedFileStream nixFileStream(nixFile);
  nixFileStream.SetCopyIfDifferent(true);
  
  if (!nixFileStream) {
    std::ostringstream msg;
    msg << "Failed to open Nix file for writing: " << nixFile;
    this->GetCMakeInstance()->IssueMessage(MessageType::FATAL_ERROR, msg.str());
    return;
  }
  
  if (this->GetCMakeInstance()->GetDebugOutput()) {
    std::cerr << "[NIX-DEBUG] Opened Nix file successfully, starting to write..." << std::endl;
  }

  // Use NixWriter for cleaner code generation
  cmNixWriter writer(nixFileStream);
  
  // Write Nix file header
  writer.WriteComment("Generated by CMake Nix Generator");
  writer.WriteLine("with import <nixpkgs> {};");
  writer.WriteLine("with pkgs;");
  writer.WriteLine("with lib;");  // Import lib for fileset functions
  writer.WriteLine();
  writer.StartLetBinding();
  
  // Write helper functions for DRY code generation
  {
    ProfileTimer helperTimer(this, "WriteNixHelperFunctions");
    this->WriteNixHelperFunctions(writer);
  }

  // Collect all custom commands with proper thread safety
  // Use temporary collections to avoid race conditions
  std::vector<CustomCommandInfo> tempCustomCommands;
  std::map<std::string, std::string> tempCustomCommandOutputs;
  std::set<std::string> processedDerivationNames;  // Track already processed derivation names
  
  // First pass: Collect all custom commands into temporary collections
  {
    ProfileTimer collectTimer(this, "CollectCustomCommands");
    for (auto const& lg : this->LocalGenerators) {
    for (auto const& target : lg->GetGeneratorTargets()) {
      if (this->GetCMakeInstance()->GetDebugOutput()) {
        std::cerr << "[NIX-DEBUG] Checking target " << target->GetName() << " for custom commands" << std::endl;
      }
      std::vector<cmSourceFile*> sources;
      target->GetSourceFiles(sources, "");
      for (const cmSourceFile* source : sources) {
        if (cmCustomCommand const* cc = source->GetCustomCommand()) {
          if (this->GetCMakeInstance()->GetDebugOutput()) {
            std::cerr << "[NIX-DEBUG] Found custom command in source: " << source->GetFullPath() << std::endl;
          }
          try {
            cmNixCustomCommandGenerator ccg(cc, target->GetLocalGenerator(), this->GetBuildConfiguration(target.get()), nullptr, nullptr);
            
            CustomCommandInfo info;
            info.DerivationName = ccg.GetDerivationName();
            info.Outputs = ccg.GetOutputs();
            info.Depends = ccg.GetDepends();
            info.Command = cc;
            info.LocalGen = target->GetLocalGenerator();
            
            // Check if we've already processed a command with this name
            if (processedDerivationNames.find(info.DerivationName) == processedDerivationNames.end()) {
              tempCustomCommands.push_back(info);
              processedDerivationNames.insert(info.DerivationName);
              
              // Populate CustomCommandOutputs map for dependency tracking
              for (const std::string& output : info.Outputs) {
                tempCustomCommandOutputs[output] = info.DerivationName;
                if (this->GetCMakeInstance()->GetDebugOutput()) {
                  std::cerr << "[NIX-DEBUG] Registering custom command output: " 
                            << output << " -> " << info.DerivationName << std::endl;
                  // Also check if this is syscall_list.h
                  if (output.find("syscall_list.h") != std::string::npos) {
                    std::cerr << "[NIX-DEBUG] !!! Found syscall_list.h output: " << output << std::endl;
                  }
                }
              }
            }
          } catch (const std::bad_alloc& e) {
            std::ostringstream msg;
            msg << "Out of memory in custom command processing for " << cc->GetComment();
            this->GetCMakeInstance()->IssueMessage(MessageType::WARNING, msg.str());
          } catch (const std::system_error& e) {
            std::ostringstream msg;
            msg << "System error in custom command processing for " << cc->GetComment()
                << ": " << e.what() << " (code: " << e.code() << ")";
            this->GetCMakeInstance()->IssueMessage(MessageType::WARNING, msg.str());
          } catch (const std::runtime_error& e) {
            std::ostringstream msg;
            msg << "Runtime error in custom command processing for " << cc->GetComment() 
                << ": " << e.what();
            this->GetCMakeInstance()->IssueMessage(MessageType::WARNING, msg.str());
          } catch (const std::exception& e) {
            std::ostringstream msg;
            msg << "Exception in custom command processing for " << cc->GetComment() 
                << ": " << e.what();
            this->GetCMakeInstance()->IssueMessage(MessageType::WARNING, msg.str());
          }
        }
      }
    }
  }
  } // End ProfileTimer collectTimer
  
  // Atomically replace the member collections with the temporary ones
  {
    std::lock_guard<std::mutex> lock(this->CustomCommandMutex);
    this->CustomCommands = std::move(tempCustomCommands);
    this->CustomCommandOutputs = std::move(tempCustomCommandOutputs);
  }
  
  // MOVED TO WriteCustomCommandDerivations - this code needs to run AFTER object derivations
  // are generated so that ObjectFileOutputs is populated
  /*
  // Create local copies for dependency processing
  std::vector<CustomCommandInfo> localCustomCommands = this->CustomCommands;
  std::map<std::string, std::string> localCustomCommandOutputs = this->CustomCommandOutputs;
  
  // Second pass: Write custom commands in dependency order
  std::set<std::string> written;
  std::vector<size_t> orderedCommands; // Store indices instead of pointers
  
  // Simple topological sort using Kahn's algorithm
  std::map<std::string, std::vector<size_t>> dependents; // Store indices
  std::map<std::string, int> inDegree;
  
  // Build dependency graph using local copies (thread-safe)
  for (const auto& info : localCustomCommands) {
    inDegree[info.DerivationName] = 0;
  }
  
  // PERFORMANCE FIX: Create map for O(1) lookup instead of O(n) search
  std::map<std::string, size_t> derivNameToIndex;
  for (size_t i = 0; i < localCustomCommands.size(); ++i) {
    derivNameToIndex[localCustomCommands[i].DerivationName] = i;
  }
  
  // Build dependency graph using indices - now O(n*m) instead of O(n²*m)
  for (size_t i = 0; i < localCustomCommands.size(); ++i) {
    const auto& info = localCustomCommands[i];
    for (const std::string& dep : info.Depends) {
      auto depIt = localCustomCommandOutputs.find(dep);
      if (depIt != localCustomCommandOutputs.end()) {
        // Use the map for O(1) lookup
        auto indexIt = derivNameToIndex.find(depIt->second);
        if (indexIt != derivNameToIndex.end()) {
          dependents[depIt->second].push_back(i);
          inDegree[info.DerivationName]++;
          break;
        }
      }
    }
  }
  
  // Find nodes with no dependencies
  std::queue<size_t> q;
  for (size_t i = 0; i < localCustomCommands.size(); ++i) {
    if (inDegree[localCustomCommands[i].DerivationName] == 0) {
      q.push(i);
    }
  }
  
  // Process in dependency order
  while (!q.empty()) {
    size_t currentIdx = q.front();
    q.pop();
    orderedCommands.push_back(currentIdx);
    
    // Reduce in-degree for dependents
    const std::string& currentName = localCustomCommands[currentIdx].DerivationName;
    for (size_t dependentIdx : dependents[currentName]) {
      if (--inDegree[localCustomCommands[dependentIdx].DerivationName] == 0) {
        q.push(dependentIdx);
      }
    }
  }
  
  // Check for cycles - if not all commands were processed, there's a cycle
  if (orderedCommands.size() != localCustomCommands.size()) {
    std::ostringstream msg;
    msg << "CMake Error: Cyclic dependency detected in custom commands. ";
    msg << "Processed " << orderedCommands.size() << " of " 
        << localCustomCommands.size() << " commands.\n\n";
    
    // Debug output
    if (this->GetCMakeInstance()->GetDebugOutput()) {
      std::cerr << "[NIX-DEBUG] Total custom commands: " << localCustomCommands.size() << std::endl;
      std::cerr << "[NIX-DEBUG] Ordered commands: " << orderedCommands.size() << std::endl;
      for (size_t i = 0; i < localCustomCommands.size(); ++i) {
        std::cerr << "[NIX-DEBUG] Command " << i << ": " << localCustomCommands[i].DerivationName << std::endl;
      }
    }
    
    // Find which commands weren't processed (part of cycles)
    std::set<std::string> processedNames;
    for (size_t idx : orderedCommands) {
      processedNames.insert(localCustomCommands[idx].DerivationName);
    }
    
    std::vector<size_t> cyclicCommands;
    for (size_t i = 0; i < localCustomCommands.size(); ++i) {
      if (processedNames.find(localCustomCommands[i].DerivationName) == processedNames.end()) {
        cyclicCommands.push_back(i);
      }
    }
    
    // More debug output  
    if (this->GetCMakeInstance()->GetDebugOutput()) {
      std::cerr << "[NIX-DEBUG] Unprocessed commands: " << cyclicCommands.size() << std::endl;
      for (size_t idx : cyclicCommands) {
        const auto& cmd = localCustomCommands[idx];
        std::cerr << "[NIX-DEBUG] Unprocessed: " << cmd.DerivationName << " (indegree=" << inDegree[cmd.DerivationName] << ")" << std::endl;
      }
    }
    
    msg << "Commands involved in circular dependencies (" << cyclicCommands.size() << " commands):\n";
    
    // Enhanced reporting with more context
    for (size_t idx : cyclicCommands) {
      const auto& info = localCustomCommands[idx];
      msg << "  • " << info.DerivationName << "\n";
      msg << "    Working directory: " << info.LocalGen->GetCurrentBinaryDirectory() << "\n";
      
      // Show the command itself (first few words)
      if (info.Command && !info.Command->GetCommandLines().empty()) {
        const auto& cmdLine = info.Command->GetCommandLines()[0];
        if (!cmdLine.empty()) {
          std::string cmdStr = cmdLine[0];
          if (cmdLine.size() > 1) {
            cmdStr += " " + cmdLine[1];
          }
          if (cmdLine.size() > 2) {
            cmdStr += " ...";
          }
          msg << "    Command: " << cmdStr << "\n";
        }
      }
      
      // Show outputs this command produces
      msg << "    Outputs: ";
      if (info.Outputs.empty()) {
        msg << "(none)";
      } else {
        for (size_t i = 0; i < info.Outputs.size(); ++i) {
          if (i > 0) msg << ", ";
          msg << cmSystemTools::GetFilenameName(info.Outputs[i]);
        }
      }
      msg << "\n";
      
      // Show dependencies this command has
      msg << "    Depends on: ";
      if (info.Depends.empty()) {
        msg << "(none)";
      } else {
        bool first = true;
        for (const std::string& dep : info.Depends) {
          if (!first) msg << ", ";
          first = false;
          
          auto depIt = this->CustomCommandOutputs.find(dep);
          if (depIt != this->CustomCommandOutputs.end()) {
            msg << depIt->second << " (via " << cmSystemTools::GetFilenameName(dep) << ")";
          } else {
            msg << cmSystemTools::GetFilenameName(dep);
          }
        }
      }
      msg << "\n\n";
    }
    
    // Try to detect and report specific cycles
    msg << "Cycle Analysis:\n";
    std::function<bool(const std::string&, std::set<std::string>&, std::vector<std::string>&, int)> findCycle;
    findCycle = [&](const std::string& current, std::set<std::string>& visited, std::vector<std::string>& path, int depth) -> bool {
      // Prevent stack overflow with depth limit
      if (depth > MAX_CYCLE_DETECTION_DEPTH) {
        this->GetCMakeInstance()->IssueMessage(MessageType::WARNING, "Cycle detection depth limit exceeded at: " + current);
        return false;
      }
      if (visited.count(current)) {
        // Found a cycle - report it
        auto cycleStart = std::find(path.begin(), path.end(), current);
        if (cycleStart != path.end()) {
          msg << "  Detected cycle: ";
          bool first = true;
          for (auto it = cycleStart; it != path.end(); ++it) {
            if (!first) msg << " → ";
            first = false;
            msg << *it;
          }
          msg << " → " << current << "\n";
          return true;
        }
      }
      
      visited.insert(current);
      path.push_back(current);
      
      // Follow dependencies
      for (size_t idx : cyclicCommands) {
        const auto& info = localCustomCommands[idx];
        if (info.DerivationName == current) {
          for (const std::string& dep : info.Depends) {
            auto depIt = localCustomCommandOutputs.find(dep);
            if (depIt != localCustomCommandOutputs.end()) {
              if (findCycle(depIt->second, visited, path, depth + 1)) {
                return true;
              }
            }
          }
          break;
        }
      }
      
      path.pop_back();
      visited.erase(current);
      return false;
    };
    
    std::set<std::string> visited;
    std::vector<std::string> path;
    bool foundCycle = false;
    for (size_t idx : cyclicCommands) {
      if (!foundCycle) {
        foundCycle = findCycle(localCustomCommands[idx].DerivationName, visited, path, 0);
      }
    }
    
    if (!foundCycle) {
      msg << "  Unable to trace specific cycle (complex interdependencies)\n";
    }
    
    msg << "\nWORKAROUND FOR COMPLEX BUILD SYSTEMS:\n";
    msg << "The Nix generator has detected circular dependencies in custom commands, which\n";
    msg << "typically occurs with complex build systems like Zephyr, Linux kernel, etc.\n";
    msg << "\n";
    msg << "To work around this issue, you can:\n";
    msg << "1. Use the Ninja generator instead: cmake -GNinja -DBOARD=native_sim/native/64 .\n";
    msg << "2. Or set CMAKE_NIX_IGNORE_CIRCULAR_DEPS=ON to bypass this check (experimental)\n";
    msg << "\n";
    msg << "GENERAL SUGGESTIONS:\n";
    msg << "• Check if custom commands have correct INPUT/OUTPUT dependencies\n";
    msg << "• Verify that generated files are not both input and output of different commands\n";
    msg << "• Consider breaking complex dependencies into separate steps\n";
    msg << "• Use add_dependencies() to establish explicit ordering when needed\n";
    
    // Check if user wants to bypass this check
    cmValue ignoreCircular = this->GetCMakeInstance()->GetCacheDefinition("CMAKE_NIX_IGNORE_CIRCULAR_DEPS");
    if (ignoreCircular && (*ignoreCircular == "ON" || *ignoreCircular == "1" || *ignoreCircular == "YES" || *ignoreCircular == "TRUE")) {
      this->GetCMakeInstance()->IssueMessage(MessageType::WARNING, 
        "Circular dependencies detected, but proceeding due to CMAKE_NIX_IGNORE_CIRCULAR_DEPS=ON\n"
        "This may result in incorrect build order and build failures.\n"
        + std::to_string(this->CustomCommands.size() - orderedCommands.size()) + 
        " commands have circular dependencies but will be processed anyway.");
      
      // Process all commands regardless of dependencies - append the unprocessed ones to orderedCommands
      for (size_t i = 0; i < this->CustomCommands.size(); ++i) {
        bool found = false;
        for (size_t idx : orderedCommands) {
          if (idx == i) {
            found = true;
            break;
          }
        }
        if (!found) {
          orderedCommands.push_back(i);
        }
      }
      
      if (this->GetCMakeInstance()->GetDebugOutput()) {
        std::cerr << "[NIX-DEBUG] Now processing all " << orderedCommands.size() << " custom commands." << std::endl;
        std::cerr << "[NIX-DEBUG] About to write custom commands to Nix file..." << std::endl;
      }
    } else {
      this->GetCMakeInstance()->IssueMessage(MessageType::FATAL_ERROR, msg.str());
      return;
    }
  }
  
  // Write commands in order
  if (this->GetCMakeInstance()->GetDebugOutput()) {
    std::cerr << "[NIX-DEBUG] Writing " << orderedCommands.size() << " custom commands" << std::endl;
    std::cerr << "[NIX-DEBUG] CustomCommandOutputs has " << this->CustomCommandOutputs.size() << " entries" << std::endl;
  }
  for (size_t idx : orderedCommands) {
    const CustomCommandInfo* info = &localCustomCommands[idx];
    try {
      std::string config = "Release";
      const auto& makefiles = this->GetCMakeInstance()->GetGlobalGenerator()->GetMakefiles();
      if (!makefiles.empty()) {
        config = makefiles[0]->GetSafeDefinition("CMAKE_BUILD_TYPE");
        if (config.empty()) {
          config = "Release";
        }
      }
      cmNixCustomCommandGenerator ccg(info->Command, info->LocalGen, config, &this->CustomCommandOutputs, &this->ObjectFileOutputs);
      ccg.Generate(nixFileStream);
    } catch (const std::bad_alloc& e) {
      std::ostringstream msg;
      msg << "Out of memory writing custom command " << info->DerivationName;
      this->GetCMakeInstance()->IssueMessage(MessageType::WARNING, msg.str());
    } catch (const std::system_error& e) {
      std::ostringstream msg;
      msg << "System error writing custom command " << info->DerivationName 
          << ": " << e.what() << " (code: " << e.code() << ")";
      this->GetCMakeInstance()->IssueMessage(MessageType::WARNING, msg.str());
    } catch (const std::runtime_error& e) {
      std::ostringstream msg;
      msg << "Runtime error writing custom command " << info->DerivationName << ": " << e.what();
      this->GetCMakeInstance()->IssueMessage(MessageType::WARNING, msg.str());
    } catch (const std::exception& e) {
      std::ostringstream msg;
      msg << "Exception writing custom command " << info->DerivationName << ": " << e.what();
      this->GetCMakeInstance()->IssueMessage(MessageType::WARNING, msg.str());
    }
  }
  */

  // Collect install targets
  this->CollectInstallTargets();

  // Write external header derivations first (before object derivations that depend on them)
  {
    ProfileTimer headerTimer(this, "WriteExternalHeaderDerivations");
    this->WriteExternalHeaderDerivations(nixFileStream);
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
          std::cerr << "[NIX-DEBUG] Target " << target->GetName() << " has " << sources.size() << " source files" << std::endl;
          for (const cmSourceFile* source : sources) {
            std::cerr << "[NIX-DEBUG]   Source: " << source->GetFullPath() 
                      << " (Unity: " << (source->GetProperty("UNITY_SOURCE_FILE") ? "yes" : "no") << ")" << std::endl;
          }
        }
        
        // Pre-create target generator and cache configuration for efficiency
        auto targetGen = cmNixTargetGenerator::New(target.get());
        std::string config = this->GetBuildConfiguration(target.get());
        
        // Pre-compute and cache library dependencies for this target
        std::pair<cmGeneratorTarget*, std::string> cacheKey = {target.get(), config};
        {
          std::lock_guard<std::mutex> lock(this->CacheMutex);
          if (this->LibraryDependencyCache.find(cacheKey) == this->LibraryDependencyCache.end()) {
            this->LibraryDependencyCache[cacheKey] = targetGen->GetTargetLibraryDependencies(config);
          }
        }
        
        for (const cmSourceFile* source : sources) {
          // Skip Unity-generated batch files (unity_X_cxx.cxx) as we don't support Unity builds
          // But still process the original source files
          std::string sourcePath = source->GetFullPath();
          if (sourcePath.find("/Unity/unity_") != std::string::npos && 
              sourcePath.find("_cxx.cxx") != std::string::npos) {
            if (this->GetCMakeInstance()->GetDebugOutput()) {
              std::cerr << "[NIX-DEBUG] Skipping Unity batch file: " << sourcePath << std::endl;
            }
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
  // Create cache key
  std::string cacheKey = targetName + "|" + sourceFile;
  
  // Check cache first
  {
    std::lock_guard<std::mutex> lock(this->CacheMutex);
    auto it = this->DerivationNameCache.find(cacheKey);
    if (it != this->DerivationNameCache.end()) {
      return it->second;
    }
  }
  
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
  {
    std::lock_guard<std::mutex> lock(this->UsedNamesMutex);
    int suffix = 2;
    while (this->UsedDerivationNames.find(finalResult) != this->UsedDerivationNames.end()) {
      finalResult = result + "_" + std::to_string(suffix);
      suffix++;
    }
    this->UsedDerivationNames.insert(finalResult);
  }
  
  // Cache the result
  {
    std::lock_guard<std::mutex> lock(this->CacheMutex);
    this->DerivationNameCache[cacheKey] = finalResult;
  }
  return finalResult;
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
    objPath = this->GetCMakeInstance()->GetHomeOutputDirectory() + "/" + objPath;
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
  cmNixWriter writer(nixFileStream);
  
  std::string sourceFile = source->GetFullPath();
  
  // Resolve symlinks to ensure the actual file is available in Nix store
  if (cmSystemTools::FileIsSymlink(sourceFile)) {
    sourceFile = cmSystemTools::GetRealPath(sourceFile);
  }
  
  if (this->GetCMakeInstance()->GetDebugOutput()) {
    std::cerr << "[NIX-DEBUG] WriteObjectDerivation for source: " << sourceFile 
              << " (generated: " << source->GetIsGenerated() << ")" << std::endl;
  }
  
  // Validate source file
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
  std::string derivName = this->GetDerivationName(target->GetName(), sourceFile);
  ObjectDerivation const& od = this->ObjectDerivations[derivName];

  std::string objectName = od.ObjectFileName;
  std::string lang = od.Language;
  std::vector<std::string> headers = od.Dependencies;
  
  // Get the configuration (Debug, Release, etc.)
  std::string config = target->Target->GetMakefile()->GetSafeDefinition("CMAKE_BUILD_TYPE");
  if (config.empty()) {
    config = "Release"; // Default configuration
  }
  
  // Get all compile flags using the helper method
  std::string allCompileFlags = this->GetCompileFlags(target, source, lang, config, objectName);
  
  // Start the derivation using cmakeNixCC helper
  nixFileStream << "  " << derivName << " = cmakeNixCC {\n";
  nixFileStream << "    name = \"" << objectName << "\";\n";
  
  // Determine source path - check if this source file is external
  std::string currentSourceDir = cmSystemTools::GetFilenamePath(sourceFile);
  std::string buildDir = this->GetCMakeInstance()->GetHomeOutputDirectory();
  std::string srcDir = this->GetCMakeInstance()->GetHomeDirectory();
  
  // Calculate relative path from build directory to source directory for out-of-source builds
  std::string projectSourceRelPath = "./.";
  if (srcDir != buildDir) {
    projectSourceRelPath = cmSystemTools::RelativePath(buildDir, srcDir);
    if (!projectSourceRelPath.empty()) {
      projectSourceRelPath = "./" + projectSourceRelPath;
      // Remove any trailing slash to avoid Nix errors
      if (projectSourceRelPath.back() == '/') {
        projectSourceRelPath.pop_back();
      }
    } else {
      projectSourceRelPath = "./.";
    }
  }
  
  std::string initialRelativePath = cmSystemTools::RelativePath(this->GetCMakeInstance()->GetHomeDirectory(), sourceFile);
  
  // Check if source file is external (outside project tree)
  bool isExternalSource = (cmNixPathUtils::IsPathOutsideTree(initialRelativePath) || cmSystemTools::FileIsFullPath(initialRelativePath));
  
  // Process files referenced by -imacros and -include flags for ALL sources (external and non-external)
  // These files (like Zephyr's autoconf.h) need to be embedded if they are configuration-time generated
  std::vector<std::string> configTimeGeneratedFiles;
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
        if (this->GetCMakeInstance()->GetDebugOutput()) {
          std::cerr << "[NIX-DEBUG] Added " << flag << " file to config-time generated: " << filePath << std::endl;
        }
      }
    }
  }
  
  // Collect custom command generated headers needed by this source BEFORE creating composite
  std::vector<std::string> customCommandHeaders;
  
  // Extract base name and extension for special case handling
  std::string baseName = cmSystemTools::GetFilenameWithoutLastExtension(sourceFile);
  std::string sourceExtension = cmSystemTools::GetFilenameLastExtension(sourceFile);
  
  // Special case: offsets.c is used to generate offsets.h, so it can't depend on offsets.h
  // This avoids circular dependencies in the build graph
  bool isOffsetsSource = (baseName == "offsets" && sourceExtension == ".c");
  
  // Even without explicit dependencies, check include directories for custom command outputs
  // This is needed for cases like Zephyr RTOS where generated headers are included
  std::vector<std::string> includeDirs;
  std::vector<std::string> parsedIncludeFlags;
  cmSystemTools::ParseUnixCommandLine(allCompileFlags.c_str(), parsedIncludeFlags);
  
  for (const std::string& flag : parsedIncludeFlags) {
    if (flag.substr(0, 2) == "-I") {
      std::string includeDir = flag.substr(2);
      if (!includeDir.empty()) {
        // Ensure absolute path
        if (!cmSystemTools::FileIsFullPath(includeDir)) {
          // Include paths can be relative to either the source or build directory
          // Try build directory first (most common for generated files)
          std::string topBuildDir = this->GetCMakeInstance()->GetHomeOutputDirectory();
          std::string topSrcDir = this->GetCMakeInstance()->GetHomeDirectory();
          
          // If the path starts with "build/", it's likely relative to the source directory
          // (as in Zephyr RTOS where build/ is a subdirectory of the source)
          if (includeDir.find("build/") == 0) {
            includeDir = topSrcDir + "/" + includeDir;
          } else {
            // Otherwise, it's relative to the build directory
            includeDir = topBuildDir + "/" + includeDir;
          }
        }
        includeDirs.push_back(includeDir);
      }
    }
  }
  
  // Debug: log all custom command outputs available
  if (this->GetCMakeInstance()->GetDebugOutput() && sourceFile.find("offsets.c") != std::string::npos) {
    std::cerr << "[NIX-DEBUG] Processing offsets.c - checking for custom command headers" << std::endl;
    std::cerr << "[NIX-DEBUG] Source file: " << sourceFile << std::endl;
    std::cerr << "[NIX-DEBUG] Build dir: " << buildDir << std::endl;
    std::cerr << "[NIX-DEBUG] Current binary dir: " << target->GetLocalGenerator()->GetCurrentBinaryDirectory() << std::endl;
    std::cerr << "[NIX-DEBUG] Total custom command outputs: " << this->CustomCommandOutputs.size() << std::endl;
    std::cerr << "[NIX-DEBUG] Include directories:" << std::endl;
    for (const auto& inc : includeDirs) {
      std::cerr << "[NIX-DEBUG]   " << inc << std::endl;
    }
    std::cerr << "[NIX-DEBUG] Custom command outputs containing 'syscall':" << std::endl;
    for (const auto& [output, deriv] : this->CustomCommandOutputs) {
      if (output.find("syscall") != std::string::npos) {
        std::cerr << "[NIX-DEBUG]   " << output << " -> " << deriv << std::endl;
      }
    }
    
    // Also check include processing
    std::cerr << "[NIX-DEBUG] Looking for the include dir containing syscall_list.h:" << std::endl;
    std::string targetInclude = "/home/mpedersen/topics/cmake_nix_backend/CMake/test_zephyr_rtos/zephyr/samples/posix/philosophers/build/zephyr/include/generated/zephyr";
    for (const auto& inc : includeDirs) {
      if (inc == targetInclude) {
        std::cerr << "[NIX-DEBUG]   FOUND matching include directory!" << std::endl;
      }
    }
  }
  
  // Check all custom command outputs to see if they're in any include directories
  for (const auto& [output, deriv] : this->CustomCommandOutputs) {
    std::string outputDir = cmSystemTools::GetFilenamePath(output);
    
    // Check if this output is in any of our include directories
    for (const std::string& includeDir : includeDirs) {
      // Resolve both paths to handle relative paths correctly
      std::string fullOutputDir = cmSystemTools::CollapseFullPath(outputDir);
      std::string fullIncludeDir = cmSystemTools::CollapseFullPath(includeDir);
      
      if (this->GetCMakeInstance()->GetDebugOutput() && output.find("syscall") != std::string::npos) {
        std::cerr << "[NIX-DEBUG] Checking custom command output: " << output << std::endl;
        std::cerr << "[NIX-DEBUG]   Output dir: " << fullOutputDir << std::endl;
        std::cerr << "[NIX-DEBUG]   Checking against include dir: " << fullIncludeDir << std::endl;
      }
      if (outputDir == fullIncludeDir || 
          cmSystemTools::IsSubDirectory(output, fullIncludeDir)) {
        // Skip offsets.h when building offsets.c to avoid circular dependencies
        if (isOffsetsSource && output.find("offsets.h") != std::string::npos) {
          if (this->GetCMakeInstance()->GetDebugOutput()) {
            std::cerr << "[NIX-DEBUG] Skipping offsets.h for offsets.c to avoid circular dependency" << std::endl;
          }
          continue;
        }
        
        // This header is in an include directory, add it as a dependency
        if (std::find(customCommandHeaders.begin(), customCommandHeaders.end(), deriv) == customCommandHeaders.end()) {
          customCommandHeaders.push_back(deriv);
          if (this->GetCMakeInstance()->GetDebugOutput()) {
            std::cerr << "[NIX-DEBUG] Found custom command header in include dir: " 
                      << output << " -> " << deriv << std::endl;
          }
        }
        break;
      }
    }
  }
  
  // Also check for headers that might be included via relative paths
  if (source) {
    // Read the source file to check for includes
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
              if (this->GetCMakeInstance()->GetDebugOutput()) {
                std::cerr << "[NIX-DEBUG] Found custom command header for composite source: " 
                          << pathToCheck << " -> " << customIt->second << std::endl;
              }
              break;
            }
          }
        }
      }
    }
  }

  // Write src attribute
  if (isExternalSource) {
    // For external sources, create a composite source including both project and external file
    // If we have config-time generated files, we need to write a composite source manually
    // since WriteCompositeSource doesn't handle external sources properly
    if (!configTimeGeneratedFiles.empty()) {
      // Create a composite source that includes project files, external source, and config-time generated files
      // Now include custom command headers in buildInputs
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
      nixFileStream << "      cp -rL ${" << projectSourceRelPath << "}/* $out/ 2>/dev/null || true\n";
      
      // Copy configuration-time generated files to their correct locations
      nixFileStream << "      # Copy configuration-time generated files\n";
      for (const auto& genFile : configTimeGeneratedFiles) {
        // Calculate the relative path within the build directory
        std::string relPath = cmSystemTools::RelativePath(buildDir, genFile);
        std::string destDir = cmSystemTools::GetFilenamePath(relPath);
        
        // Read the file content and embed it directly
        cmsys::ifstream inFile(genFile.c_str(), std::ios::in | std::ios::binary);
        if (inFile) {
          std::ostringstream contents;
          contents << inFile.rdbuf();
          inFile.close();
          
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
      
      // Handle external include directories from compile flags
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
      
      // Update compile flags for external include directories
      for (const auto& inc : includes) {
        if (!inc.Value.empty()) {
          std::string incPath = inc.Value;
          if (cmSystemTools::FileIsFullPath(incPath)) {
            std::string relPath = cmSystemTools::RelativePath(srcDir, incPath);
            if (cmNixPathUtils::IsPathOutsideTree(relPath)) {
              // Normalize the path
              std::string normalizedPath = cmSystemTools::CollapseFullPath(incPath);
              
              // Find and replace -I<absolute_path> with -I<relative_path>
              std::string searchStr = "-I" + normalizedPath;
              std::string replaceStr = "-I" + normalizedPath.substr(1); // Remove leading /
              
              size_t pos = 0;
              while ((pos = allCompileFlags.find(searchStr, pos)) != std::string::npos) {
                allCompileFlags.replace(pos, searchStr.length(), replaceStr);
                pos += replaceStr.length();
              }
              
              if (this->GetCMakeInstance()->GetDebugOutput()) {
                std::cerr << "[NIX-DEBUG] Replaced " << searchStr << " with " << replaceStr << " in compile flags" << std::endl;
              }
            }
          }
        }
      }
      
      // Copy the external source file
      std::string fileName = cmSystemTools::GetFilenameName(sourceFile);
      nixFileStream << "      # Copy external source file\n";
      nixFileStream << "      cp ${builtins.path { path = \"" << sourceFile << "\"; }} $out/" << fileName << "\n";
      
      // For ABI detection files, also copy the required header file
      if (fileName.find("CMakeCCompilerABI.c") != std::string::npos ||
          fileName.find("CMakeCXXCompilerABI.cpp") != std::string::npos) {
        std::string abiSourceDir = cmSystemTools::GetFilenamePath(sourceFile);
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
      std::string headerDerivName;
      if (!externalHeaders.empty()) {
        std::string sourceDir = cmSystemTools::GetFilenamePath(sourceFile);
        headerDerivName = this->GetOrCreateHeaderDerivation(sourceDir, externalHeaders);
        
        // Store mapping from source file to header derivation
        {
          std::lock_guard<std::mutex> lock(this->ExternalHeaderMutex);
          this->SourceToHeaderDerivation[sourceFile] = headerDerivName;
        }
        
        // Symlink headers from the header derivation
        nixFileStream << "      # Link headers from external header derivation\n";
        nixFileStream << "      if [ -d ${" << headerDerivName << "} ]; then\n";
        nixFileStream << "        cp -rL ${" << headerDerivName << "}/* $out/ 2>/dev/null || true\n";
        nixFileStream << "      fi\n";
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
          
          // Find the actual output path for this derivation
          for (const auto& [output, deriv] : this->CustomCommandOutputs) {
            if (deriv == headerDeriv) {
              std::string relativePath = cmSystemTools::RelativePath(buildDir, output);
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
    } else {
      // No config-time generated files, use simple approach
      // Include custom command headers in buildInputs
      nixFileStream << "    src = pkgs.runCommand \"composite-src\" {\n";
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
      // Copy project source tree (use -L to follow symlinks and avoid permission issues)
      nixFileStream << "      cp -rL ${" << projectSourceRelPath << "}/* $out/ 2>/dev/null || true\n";
      
      // Handle external include directories from compile flags
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
      
      // Update compile flags for external include directories
      for (const auto& inc : includes) {
        if (!inc.Value.empty()) {
          std::string incPath = inc.Value;
          if (cmSystemTools::FileIsFullPath(incPath)) {
            std::string relPath = cmSystemTools::RelativePath(srcDir, incPath);
            if (cmNixPathUtils::IsPathOutsideTree(relPath)) {
              // Normalize the path
              std::string normalizedPath = cmSystemTools::CollapseFullPath(incPath);
              
              // Find and replace -I<absolute_path> with -I<relative_path>
              std::string searchStr = "-I" + normalizedPath;
              std::string replaceStr = "-I" + normalizedPath.substr(1); // Remove leading /
              
              size_t pos = 0;
              while ((pos = allCompileFlags.find(searchStr, pos)) != std::string::npos) {
                allCompileFlags.replace(pos, searchStr.length(), replaceStr);
                pos += replaceStr.length();
              }
              
              if (this->GetCMakeInstance()->GetDebugOutput()) {
                std::cerr << "[NIX-DEBUG] Replaced " << searchStr << " with " << replaceStr << " in compile flags" << std::endl;
              }
            }
          }
        }
      }
      
      // Copy external source file to build dir root
      std::string fileName = cmSystemTools::GetFilenameName(sourceFile);
      // Use builtins.path for absolute paths
      nixFileStream << "      cp ${builtins.path { path = \"" << sourceFile << "\"; }} $out/" << fileName << "\n";
      
      // For ABI detection files, also copy the required header file
      if (fileName.find("CMakeCCompilerABI.c") != std::string::npos ||
          fileName.find("CMakeCXXCompilerABI.cpp") != std::string::npos) {
        std::string abiSourceDir = cmSystemTools::GetFilenamePath(sourceFile);
        std::string abiHeaderFile = abiSourceDir + "/CMakeCompilerABI.h";
        nixFileStream << "      cp ${builtins.path { path = \"" << abiHeaderFile << "\"; }} $out/CMakeCompilerABI.h\n";
      }
      
      // Get header dependencies for external source
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
      std::string headerDerivName;
      if (!externalHeaders.empty()) {
        std::string sourceDir = cmSystemTools::GetFilenamePath(sourceFile);
        headerDerivName = this->GetOrCreateHeaderDerivation(sourceDir, externalHeaders);
        
        // Store mapping from source file to header derivation
        {
          std::lock_guard<std::mutex> lock(this->ExternalHeaderMutex);
          this->SourceToHeaderDerivation[sourceFile] = headerDerivName;
        }
        
        // Symlink headers from the header derivation
        nixFileStream << "      # Link headers from external header derivation\n";
        nixFileStream << "      if [ -d ${" << headerDerivName << "} ]; then\n";
        nixFileStream << "        cp -rL ${" << headerDerivName << "}/* $out/ 2>/dev/null || true\n";
        nixFileStream << "      fi\n";
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
          
          // Find the actual output path for this derivation
          for (const auto& [output, deriv] : this->CustomCommandOutputs) {
            if (deriv == headerDeriv) {
              std::string relativePath = cmSystemTools::RelativePath(buildDir, output);
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
  } else {
    // Regular project source - always use fileset for better performance
    auto targetGen = cmNixTargetGenerator::New(target);
    std::vector<std::string> dependencies = targetGen->GetSourceDependencies(source);
    
    if (this->GetCMakeInstance()->GetDebugOutput()) {
      std::cerr << "[NIX-DEBUG] Source dependencies for " << sourceFile << ": " << dependencies.size() << std::endl;
      for (const auto& dep : dependencies) {
        std::cerr << "[NIX-DEBUG]   Dependency: " << dep << std::endl;
      }
    }
    
    // Create lists for existing and generated files
    std::vector<std::string> existingFiles;
    std::vector<std::string> generatedFiles;
    
    // Add the main source file
    std::string relativeSource = cmSystemTools::RelativePath(
      this->GetCMakeInstance()->GetHomeDirectory(), sourceFile);
    if (!relativeSource.empty() && relativeSource.find("../") != 0) {
      if (source->GetIsGenerated()) {
        generatedFiles.push_back(relativeSource);
      } else {
        existingFiles.push_back(relativeSource);
      }
    }
    
    // Process header dependencies using helper method
    if (this->GetCMakeInstance()->GetDebugOutput()) {
      std::cerr << "[NIX-DEBUG] Processing headers for " << sourceFile << ": " << dependencies.size() << " headers" << std::endl;
    }
    this->ProcessHeaderDependencies(dependencies, buildDir, srcDir, 
                                   existingFiles, generatedFiles, configTimeGeneratedFiles);
    
    // Note: PCH header file handling is now done in GetCompileFlags helper
    // Note: -imacros and -include processing has been moved to the top for both external and non-external sources
    
    // Check if we need a composite source (for config-time generated files or external includes)
    bool hasExternalIncludes = false;
    cmLocalGenerator* lg = target->GetLocalGenerator();
    std::vector<BT<std::string>> includes = lg->GetIncludeDirectories(target, lang, config);
    for (const auto& inc : includes) {
      if (!inc.Value.empty()) {
        std::string incPath = inc.Value;
        if (cmSystemTools::FileIsFullPath(incPath)) {
          std::string relPath = cmSystemTools::RelativePath(srcDir, incPath);
          if (cmNixPathUtils::IsPathOutsideTree(relPath)) {
            hasExternalIncludes = true;
            break;
          }
        }
      }
    }
    
    // Handle configuration-time generated files or external includes
    if (!configTimeGeneratedFiles.empty() || hasExternalIncludes || !customCommandHeaders.empty()) {
      this->WriteCompositeSource(nixFileStream, configTimeGeneratedFiles, srcDir, buildDir, target, lang, config, customCommandHeaders);
      
      // Note: Compile flag updating for config-time generated files is now done later for ALL sources
      
      // Also update compile flags for external include directories
      if (hasExternalIncludes) {
        for (const auto& inc : includes) {
          if (!inc.Value.empty()) {
            std::string incPath = inc.Value;
            if (cmSystemTools::FileIsFullPath(incPath)) {
              std::string relPath = cmSystemTools::RelativePath(srcDir, incPath);
              if (cmNixPathUtils::IsPathOutsideTree(relPath)) {
                // Normalize the path
                std::string normalizedPath = cmSystemTools::CollapseFullPath(incPath);
                
                // Find and replace -I<absolute_path> with -I<relative_path>
                std::string searchStr = "-I" + normalizedPath;
                std::string replaceStr = "-I" + normalizedPath.substr(1); // Remove leading /
                
                size_t pos = 0;
                while ((pos = allCompileFlags.find(searchStr, pos)) != std::string::npos) {
                  allCompileFlags.replace(pos, searchStr.length(), replaceStr);
                  pos += replaceStr.length();
                }
                
                if (this->GetCMakeInstance()->GetDebugOutput()) {
                  std::cerr << "[NIX-DEBUG] Replaced " << searchStr << " with " << replaceStr << " in compile flags" << std::endl;
                }
              }
            }
          }
        }
      }
    } else if (existingFiles.empty() && generatedFiles.empty()) {
      // No files detected, use whole directory
      // Calculate relative path from build directory to source directory for out-of-source builds
      std::string srcDirLocal2 = this->GetCMakeInstance()->GetHomeDirectory();
      std::string bldDir = this->GetCMakeInstance()->GetHomeOutputDirectory();
      std::string rootPath = "./.";
      if (srcDirLocal2 != bldDir) {
        rootPath = cmSystemTools::RelativePath(bldDir, srcDirLocal2);
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
      nixFileStream << "    src = " << rootPath << ";\n";
    } else {
      // Always use fileset union for minimal source sets to avoid unnecessary rebuilds
      // Note: When CMAKE_NIX_EXPLICIT_SOURCES is OFF, we include the source file only
      // (no header dependencies) but still use a fileset to minimize rebuilds
      if (!this->UseExplicitSources() && existingFiles.size() + generatedFiles.size() > 0) {
        // When not using explicit sources, only include the source file itself
        // Clear any header dependencies to simplify the fileset
        existingFiles.clear();
        generatedFiles.clear();
        
        // Re-add just the main source file
        std::string relSource = cmSystemTools::RelativePath(
          this->GetCMakeInstance()->GetHomeDirectory(), sourceFile);
        if (!relSource.empty() && relSource.find("../") != 0) {
          if (source->GetIsGenerated()) {
            generatedFiles.push_back(relSource);
          } else {
            existingFiles.push_back(relSource);
          }
        }
        
        // Also add include directories that are part of the project
        // This ensures headers can be found during compilation
        // Note: lg and includes are already defined above, reuse them
        for (const auto& inc : includes) {
          if (!inc.Value.empty()) {
            std::string incPath = inc.Value;
            // Only add project-relative include directories
            if (!cmSystemTools::FileIsFullPath(incPath)) {
              // This is a relative path, check if it exists in the project
              std::string fullIncPath = this->GetCMakeInstance()->GetHomeDirectory() + "/" + incPath;
              if (cmSystemTools::FileExists(fullIncPath) && cmSystemTools::FileIsDirectory(fullIncPath)) {
                // Add the include directory to the fileset
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
        
        // Also add the source file's directory if it's not already included
        // This handles cases where headers are in the same directory as sources
        std::string sourceDir = cmSystemTools::GetFilenamePath(relSource);
        if (sourceDir.empty()) {
          // Source is in the root directory
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
            // Get all header files in the source directory
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
        // Calculate relative path from build directory to source directory for out-of-source builds
        std::string srcDirLocal4 = this->GetCMakeInstance()->GetHomeDirectory();
        std::string bldDir = this->GetCMakeInstance()->GetHomeOutputDirectory();
        std::string rootPath = "./.";
        if (srcDirLocal4 != bldDir) {
          rootPath = cmSystemTools::RelativePath(bldDir, srcDirLocal4);
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
        
        // Use fileset union helper
        this->WriteFilesetUnion(nixFileStream, existingFiles, generatedFiles, rootPath);
      } else {
        // Fallback to whole directory if no files were collected
        // Calculate relative path from build directory to source directory for out-of-source builds
        std::string srcDirLocal5 = this->GetCMakeInstance()->GetHomeDirectory();
        std::string bldDir = this->GetCMakeInstance()->GetHomeOutputDirectory();
        std::string rootPath = "./.";
        if (srcDirLocal5 != bldDir) {
          rootPath = cmSystemTools::RelativePath(bldDir, srcDirLocal5);
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
        nixFileStream << "    src = " << rootPath << ";\n";
      }
    }
  }
  
  // Build buildInputs list using helper method
  std::vector<std::string> buildInputs = this->BuildBuildInputsList(target, source, config, sourceFile, projectSourceRelPath);
  
  // Write buildInputs attribute
  if (!buildInputs.empty()) {
    nixFileStream << "    buildInputs = [ ";
    for (size_t i = 0; i < buildInputs.size(); ++i) {
      if (i > 0) nixFileStream << " ";
      nixFileStream << buildInputs[i];
    }
    nixFileStream << " ];\n";
  }
  
  // Filter project headers using helper method
  std::vector<std::string> projectHeaders = this->FilterProjectHeaders(headers);
  
  // Note: We don't use propagatedInputs for header dependencies because:
  // 1. Headers are already included in the fileset union for the source
  // 2. Relative paths with .. segments in propagatedInputs cause Nix evaluation errors
  // 3. The actual dependency tracking is handled by the fileset, not propagatedInputs
  //
  // The following code is commented out but kept for reference:
  // if (!projectHeaders.empty()) {
  //   writer.WriteComment("Header dependencies");
  //   std::vector<std::string> propagatedInputsList;
  //   for (const std::string& header : projectHeaders) {
  //     if (isExternalSource) {
  //       // For external sources, headers are copied into the composite source root
  //       propagatedInputsList.push_back("./" + header);
  //     } else {
  //       // For project sources, headers are relative to the source directory
  //       if (projectSourceRelPath.empty()) {
  //         // Source is current directory, headers are relative to it
  //         propagatedInputsList.push_back("./" + header);
  //       } else {
  //         // Source is a subdirectory (e.g., ext/opencv), headers are relative to that source directory
  //         // Use the same source path as the derivation
  //         propagatedInputsList.push_back(projectSourceRelPath + "/" + header);
  //       }
  //     }
  //   }
  //   writer.WriteListAttribute("propagatedInputs", propagatedInputsList);
  // }
  
  // Determine the source path - always use source directory as base
  std::string sourcePath;
  std::string customCommandDep;
  auto customIt = this->CustomCommandOutputs.find(sourceFile);
  if (customIt != this->CustomCommandOutputs.end()) {
    customCommandDep = customIt->second;
  }
  
  if (!customCommandDep.empty()) {
    // Source is generated by a custom command - reference from derivation output
    // Use the top-level build directory as the base for consistent path resolution
    // This matches what cmNixCustomCommandGenerator uses
    std::string topBuildDir = this->GetCMakeInstance()->GetHomeOutputDirectory();
    std::string relativePath = cmSystemTools::RelativePath(topBuildDir, sourceFile);
    sourcePath = "${" + customCommandDep + "}/" + relativePath;
  } else {
    // All files (source and generated) - use relative path from source directory
    std::string projectSourceDir = this->GetCMakeInstance()->GetHomeDirectory();
    std::string projectBuildDir = this->GetCMakeInstance()->GetHomeOutputDirectory();
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
  
  // Don't escape Nix expressions (those containing ${...})
  if (sourcePath.find("${") != std::string::npos) {
    nixFileStream << "    source = \"" << sourcePath << "\";\n";
  } else {
    nixFileStream << "    source = \"" << cmNixWriter::EscapeNixString(sourcePath) << "\";\n";
  }
  
  // Write compiler attribute (get from buildInputs[0])
  std::string defaultCompiler = this->GetCompilerPackage(lang);
  nixFileStream << "    compiler = " << (!buildInputs.empty() ? buildInputs[0] : defaultCompiler) << ";\n";
  
  // Update compile flags to use relative paths for embedded config-time generated files
  // This is needed for ALL sources (external and non-external)
  for (const auto& genFile : configTimeGeneratedFiles) {
    std::string absPath = genFile;
    std::string relPath = cmSystemTools::RelativePath(buildDir, genFile);
    
    // Replace absolute path with relative path in compile flags
    size_t pos = 0;
    while ((pos = allCompileFlags.find(absPath, pos)) != std::string::npos) {
      allCompileFlags.replace(pos, absPath.length(), relPath);
      pos += relPath.length();
    }
    
    if (this->GetCMakeInstance()->GetDebugOutput()) {
      std::cerr << "[NIX-DEBUG] Replaced " << absPath << " with " << relPath << " in compile flags" << std::endl;
    }
  }
  
  // Add -fPIC for shared and module libraries if not already present
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
  
  // Write flags attribute
  if (!allFlags.empty()) {
    nixFileStream << "    flags = \"" << cmNixWriter::EscapeNixString(allFlags) << "\";\n";
  }
  
  // Close the derivation
  nixFileStream << "  };\n\n";
}

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
  std::string lang = source->GetLanguage();
  
  // First check if user has set CMAKE_NIX_<LANG>_COMPILER_PACKAGE
  std::string compilerPkgVar = "CMAKE_NIX_" + lang + "_COMPILER_PACKAGE";
  cmValue userPkg = target->Target->GetMakefile()->GetDefinition(compilerPkgVar);
  if (userPkg && !userPkg->empty()) {
    return *userPkg;
  }
  
  // Otherwise use default mapping
  return this->GetCompilerPackage(lang);
}

std::string cmGlobalNixGenerator::GetCompileFlags(cmGeneratorTarget* target,
                                                   const cmSourceFile* source,
                                                   const std::string& lang,
                                                   const std::string& config,
                                                   const std::string& objectName)
{
  cmLocalGenerator* lg = target->GetLocalGenerator();
  
  // Get source and build directories upfront (needed for path processing)
  std::string sourceDir = this->GetCMakeInstance()->GetHomeDirectory();
  std::string buildDir = this->GetCMakeInstance()->GetHomeOutputDirectory();
  
  // Get configuration-specific compile flags
  std::vector<BT<std::string>> compileFlagsVec = lg->GetTargetCompileFlags(target, config, lang, "");
  std::ostringstream compileFlagsStream;
  bool firstFlag = true;
  
  if (this->GetCMakeInstance()->GetDebugOutput()) {
    std::cerr << "[NIX-DEBUG] GetCompileFlags called for " << objectName << std::endl;
    std::cerr << "[NIX-DEBUG] Number of compile flags: " << compileFlagsVec.size() << std::endl;
  }
  
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
          
          if (this->GetCMakeInstance()->GetDebugOutput()) {
            std::cerr << "[NIX-DEBUG] Processing " << pFlag << " flag with file: " << filePath << std::endl;
            std::cerr << "[NIX-DEBUG] buildDir: " << buildDir << std::endl;
            std::cerr << "[NIX-DEBUG] sourceDir: " << sourceDir << std::endl;
          }
          
          // Check if it's an absolute path that needs to be made relative
          if (cmSystemTools::FileIsFullPath(filePath)) {
            // Check if it's in the build directory (configuration-time generated)
            std::string relToBuild = cmSystemTools::RelativePath(buildDir, filePath);
            if (this->GetCMakeInstance()->GetDebugOutput()) {
              std::cerr << "[NIX-DEBUG] relToBuild: " << relToBuild << std::endl;
              std::cerr << "[NIX-DEBUG] IsPathOutsideTree: " << cmNixPathUtils::IsPathOutsideTree(relToBuild) << std::endl;
            }
            if (!cmNixPathUtils::IsPathOutsideTree(relToBuild)) {
              // This is a build directory file - for configuration-time generated files
              // that will be embedded, just use the relative path from build dir
              filePath = relToBuild;
              if (this->GetCMakeInstance()->GetDebugOutput()) {
                std::cerr << "[NIX-DEBUG] Converted to build-relative path: " << filePath << std::endl;
              }
            } else {
              // Check if it's in the source directory
              std::string relToSource = cmSystemTools::RelativePath(sourceDir, filePath);
              if (!cmNixPathUtils::IsPathOutsideTree(relToSource)) {
                // This is a source directory file - use relative path
                filePath = relToSource;
                if (this->GetCMakeInstance()->GetDebugOutput()) {
                  std::cerr << "[NIX-DEBUG] Converted to source-relative path: " << filePath << std::endl;
                }
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
  
  std::string derivName = this->GetDerivationName(target->GetName());
  std::string targetName = target->GetName();
  
  // Determine source path for subdirectory adjustment
  std::string sourceDir = this->GetCMakeInstance()->GetHomeDirectory();
  std::string buildDir = this->GetCMakeInstance()->GetHomeOutputDirectory();
  std::string projectSourceRelPath = cmSystemTools::RelativePath(buildDir, sourceDir);
  
  // Debug output
  if (this->GetCMakeInstance()->GetDebugOutput()) {
    std::cerr << "[NIX-DEBUG] WriteLinkDerivation: sourceDir=" << sourceDir 
              << ", buildDir=" << buildDir << ", projectSourceRelPath=" << projectSourceRelPath << std::endl;
  }
  
  // Check if this is a try_compile
  bool isTryCompile = buildDir.find("CMakeScratch") != std::string::npos;
  
  if (this->GetCMakeInstance()->GetDebugOutput()) {
    std::cerr << "[NIX-DEBUG] " << __FILE__ << ":" << __LINE__ 
              << " WriteLinkDerivation for target: " << targetName
              << " buildDir: " << buildDir
              << " isTryCompile: " << (isTryCompile ? "true" : "false") << std::endl;
  }
  
  // Use NixWriter for cleaner output
  cmNixWriter writer(nixFileStream);
  
  // Generate appropriate name for target type
  std::string outputName;
  if (target->GetType() == cmStateEnums::SHARED_LIBRARY) {
    outputName = this->GetLibraryPrefix() + targetName + this->GetSharedLibraryExtension();
  } else if (target->GetType() == cmStateEnums::MODULE_LIBRARY) {
    outputName = targetName + this->GetSharedLibraryExtension();  // Modules typically don't have lib prefix
  } else {
    outputName = targetName;
  }
  
  // Map target type to cmakeNixLD type parameter
  std::string nixTargetType;
  if (target->GetType() == cmStateEnums::STATIC_LIBRARY) {
    nixTargetType = "static";
  } else if (target->GetType() == cmStateEnums::SHARED_LIBRARY) {
    nixTargetType = "shared";
  } else if (target->GetType() == cmStateEnums::MODULE_LIBRARY) {
    nixTargetType = "module";
  } else {
    nixTargetType = "executable";
  }
  
  // Start derivation using cmakeNixLD helper
  nixFileStream << "  " << derivName << " = cmakeNixLD {\n";
  // For cmakeNixLD, always use the base target name without prefix/extension
  // The helper will add the appropriate prefix and extension based on the type
  nixFileStream << "    name = \"" << targetName << "\";\n";
  nixFileStream << "    type = \"" << nixTargetType << "\";\n";
  
  // Get external library dependencies
  std::string config = this->GetBuildConfiguration(target);
  std::vector<std::string> libraryDeps = this->GetCachedLibraryDependencies(target, config);
  
  // Get link implementation for dependency processing
  auto linkImpl = target->GetLinkImplementation(config, cmGeneratorTarget::UseTo::Compile);
  
  // Build buildInputs list including external libraries
  // Determine the primary language for linking
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
  
  // Build buildInputs list
  std::vector<std::string> buildInputs;
  std::string compilerPkg = this->GetCompilerPackage(primaryLang);
  buildInputs.push_back(compilerPkg);
  
  // Add external library dependencies
  this->ProcessLibraryDependenciesForBuildInputs(libraryDeps, buildInputs, projectSourceRelPath);
  
  // Get transitive shared library dependencies (exclude those already direct)
  std::set<std::string> transitiveDeps = this->DependencyGraph.GetTransitiveSharedLibraries(targetName);
  std::set<std::string> directSharedDeps;
  
  // Add direct CMake target dependencies (only shared libraries)
  if (linkImpl) {
    for (const cmLinkItem& item : linkImpl->Libraries) {
      if (item.Target && !item.Target->IsImported()) {
        // Only add shared and module libraries to buildInputs, not static libraries
        if (item.Target->GetType() == cmStateEnums::SHARED_LIBRARY ||
            item.Target->GetType() == cmStateEnums::MODULE_LIBRARY) {
          std::string depTargetName = item.Target->GetName();
          std::string depDerivName = this->GetDerivationName(depTargetName);
          buildInputs.push_back(depDerivName);
          directSharedDeps.insert(depTargetName); // Track direct deps to avoid duplication
        }
      }
    }
  }
  
  // Add transitive shared library dependencies to buildInputs (excluding direct ones)
  for (const std::string& depTarget : transitiveDeps) {
    if (directSharedDeps.find(depTarget) == directSharedDeps.end()) {
      std::string depDerivName = this->GetDerivationName(depTarget);
      buildInputs.push_back(depDerivName);
    }
  }
  
  // Write buildInputs list
  nixFileStream << "    buildInputs = [ ";
  bool first = true;
  for (const std::string& input : buildInputs) {
    if (!first) nixFileStream << " ";
    nixFileStream << input;
    first = false;
  }
  nixFileStream << " ];\n";
  
  // Collect object file dependencies (reuse sources from above)
  nixFileStream << "    objects = [ ";
  
  // Get PCH sources to exclude from linking
  std::unordered_set<std::string> pchSources;
  std::set<std::string> languages;
  target->GetLanguages(languages, config);
  for (const std::string& lang : languages) {
    std::vector<std::string> pchArchs = target->GetPchArchs(config, lang);
    for (const std::string& arch : pchArchs) {
      std::string pchSource = target->GetPchSource(config, lang, arch);
      if (!pchSource.empty()) {
        pchSources.insert(pchSource);
      }
    }
  }
  
  bool firstObject = true;
  for (cmSourceFile* source : sources) {
    // Skip Unity-generated batch files (unity_X_cxx.cxx) as we don't support Unity builds
    // But still process the original source files
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
        if (!firstObject) nixFileStream << " ";
        nixFileStream << objDerivName;
        firstObject = false;
      }
    }
  }
  
  // Add object files from OBJECT libraries referenced by $<TARGET_OBJECTS:...>
  std::vector<cmSourceFile const*> externalObjects;
  target->GetExternalObjects(externalObjects, config);
  for (cmSourceFile const* source : externalObjects) {
    // External objects come from OBJECT libraries
    // The path ends with .o but we need to find the corresponding source file
    std::string objectFile = source->GetFullPath();
    
    // Remove .o extension to get the source file path
    std::string sourceFile = objectFile;
    std::string objExt = this->GetObjectFileExtension();
    if (sourceFile.size() > objExt.size() && sourceFile.substr(sourceFile.size() - objExt.size()) == objExt) {
      sourceFile = sourceFile.substr(0, sourceFile.size() - objExt.size());
    }
    
    // Find the OBJECT library that contains this source
    for (auto const& lg : this->LocalGenerators) {
      auto const& targets = lg->GetGeneratorTargets();
      for (auto const& objTarget : targets) {
        if (objTarget->GetType() == cmStateEnums::OBJECT_LIBRARY) {
          std::vector<cmSourceFile*> objSources;
          objTarget->GetSourceFiles(objSources, config);
          for (cmSourceFile* objSource : objSources) {
            if (objSource->GetFullPath() == sourceFile) {
              // Found the OBJECT library that contains this source
              std::string objDerivName = this->GetDerivationName(
                objTarget->GetName(), sourceFile);
              if (!firstObject) nixFileStream << " ";
              nixFileStream << objDerivName;
              firstObject = false;
              goto next_external_object;
            }
          }
        }
      }
    }
    next_external_object:;
  }
  
  nixFileStream << " ];\n";
  
  // Get compiler package and command based on primary language
  nixFileStream << "    compiler = " << compilerPkg << ";\n";
  
  // Pass the primary language to help select the right compiler binary
  std::string compilerCommand = this->GetCompilerCommand(primaryLang);
  // Only write compilerCommand if it differs from the default (package name)
  if (compilerCommand != compilerPkg) {
    nixFileStream << "    compilerCommand = \"" << compilerCommand << "\";\n";
  }
  // For C and other languages, the default logic in cmakeNixLD will work
  
  // Get library link flags for build phase - use vector for efficient concatenation
  std::vector<std::string> linkFlagsList;
  
  // Use helper method to process library dependencies
  this->ProcessLibraryDependenciesForLinking(target, config, linkFlagsList, transitiveDeps);
  
  // Get library list for cmakeNixLD helper
  std::vector<std::string> libraries;
  for (const std::string& depTarget : transitiveDeps) {
    if (directSharedDeps.find(depTarget) == directSharedDeps.end()) {
      std::string depDerivName = this->GetDerivationName(depTarget);
      libraries.push_back("${" + depDerivName + "}/" + this->GetLibraryPrefix() + 
                         depTarget + this->GetSharedLibraryExtension());
    }
  }
  
  // Write flags parameter
  std::string linkFlags;
  if (!linkFlagsList.empty()) {
    linkFlags = cmJoin(linkFlagsList, " ");
  }
  if (!linkFlags.empty()) {
    nixFileStream << "    flags = \"" << linkFlags << "\";\n";
  }
  
  // Write libraries parameter
  if (!libraries.empty()) {
    nixFileStream << "    libraries = [";
    bool firstLib = true;
    for (const std::string& lib : libraries) {
      if (!firstLib) nixFileStream << " ";
      nixFileStream << "\"" << lib << "\"";
      firstLib = false;
    }
    nixFileStream << " ];\n";
  }
  
  // Get library version properties for shared libraries
  if (target->GetType() == cmStateEnums::SHARED_LIBRARY) {
    cmValue version = target->GetProperty("VERSION");
    cmValue soversion = target->GetProperty("SOVERSION");
    
    if (version) {
      nixFileStream << "    version = \"" << *version << "\";\n";
    }
    if (soversion) {
      nixFileStream << "    soversion = \"" << *soversion << "\";\n";
    }
  }
  
  // Add try_compile handling if needed
  if (isTryCompile) {
    if (this->GetCMakeInstance()->GetDebugOutput()) {
      std::cerr << "[NIX-DEBUG] " << __FILE__ << ":" << __LINE__ 
                << " Adding try_compile output file handling for: " << targetName << std::endl;
    }
    
    // For try_compile, we need to add a postBuildPhase to cmakeNixLD helper
    nixFileStream << "    # Handle try_compile COPY_FILE requirement\n";
    nixFileStream << "    postBuildPhase = ''\n";
    nixFileStream << "      # Create output location in build directory for CMake COPY_FILE\n";
    std::string escapedBuildDir = cmOutputConverter::EscapeForShell(buildDir, cmOutputConverter::Shell_Flag_IsUnix);
    std::string escapedTargetName = cmOutputConverter::EscapeForShell(targetName, cmOutputConverter::Shell_Flag_IsUnix);
    nixFileStream << "      COPY_DEST=" << escapedBuildDir << "/" << escapedTargetName << "\n";
    nixFileStream << "      cp \"$out\" \"$COPY_DEST\"\n";
    if (this->GetCMakeInstance()->GetDebugOutput()) {
      nixFileStream << "      echo '[NIX-DEBUG] Copied try_compile output to: '\"$COPY_DEST\"\n";
    }
    nixFileStream << "      # Write location file that CMake expects to find the executable path\n";
    nixFileStream << "      echo \"$COPY_DEST\" > " << escapedBuildDir << "/" << escapedTargetName << "_loc\n";
    if (this->GetCMakeInstance()->GetDebugOutput()) {
      nixFileStream << "      echo '[NIX-DEBUG] Wrote location file: '" << escapedBuildDir << "/" << escapedTargetName << "_loc\n";
      nixFileStream << "      echo '[NIX-DEBUG] Location file contains: '\"$COPY_DEST\"\n";
    }
    nixFileStream << "    '';\n";
  }
  
  // Close the cmakeNixLD helper call
  nixFileStream << "  };\n";
  nixFileStream << "\n";
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
  std::string config = target->Target->GetMakefile()->GetSafeDefinition("CMAKE_BUILD_TYPE");
  if (config.empty()) {
    config = "Release"; // Default to Release if no configuration specified
  }
  return config;
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
  
  std::pair<cmGeneratorTarget*, std::string> cacheKey = {target, config};
  
  // Double-checked locking pattern to prevent race condition
  {
    std::lock_guard<std::mutex> lock(this->CacheMutex);
    auto it = this->LibraryDependencyCache.find(cacheKey);
    if (it != this->LibraryDependencyCache.end()) {
      return it->second;
    }
  }
  
  // Compute dependencies outside the lock
  auto targetGen = cmNixTargetGenerator::New(target);
  std::vector<std::string> libraryDeps = targetGen->GetTargetLibraryDependencies(config);
  
  // Check again inside lock in case another thread computed it
  {
    std::lock_guard<std::mutex> lock(this->CacheMutex);
    // Check if another thread already inserted while we were computing
    auto it = this->LibraryDependencyCache.find(cacheKey);
    if (it != this->LibraryDependencyCache.end()) {
      // Another thread beat us, use their result
      return it->second;
    }
    // We're the first, insert our result
    this->LibraryDependencyCache[cacheKey] = libraryDeps;
  }
  
  return libraryDeps;
}

void cmGlobalNixGenerator::ProcessLibraryDependenciesForLinking(
  cmGeneratorTarget* target,
  const std::string& config,
  std::vector<std::string>& linkFlagsList,
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
        linkFlagsList.push_back("${" + depDerivName + "}/" + this->GetLibraryPrefix() + 
                               depTargetName + this->GetSharedLibraryExtension());
      } else if (item.Target->GetType() == cmStateEnums::MODULE_LIBRARY) {
        // For module libraries, use Nix string interpolation (no lib prefix)
        linkFlagsList.push_back("${" + depDerivName + "}/" + depTargetName + 
                               this->GetSharedLibraryExtension());
      } else if (item.Target->GetType() == cmStateEnums::STATIC_LIBRARY) {
        // For static libraries, link the archive directly using string interpolation
        linkFlagsList.push_back("${" + depDerivName + "}");
      }
    } else if (!item.Target) { 
      // External library (not a target)
      std::string libName = item.AsStr();
      linkFlagsList.push_back("-l" + libName);
    }
  }
  
  // Get all transitive shared library dependencies in one call
  // This is more efficient than calling GetTransitiveSharedLibraries for each direct dependency
  transitiveDeps = this->DependencyGraph.GetTransitiveSharedLibraries(target->GetName());
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
  for (cmGeneratorTarget* target : this->InstallTargets) {
    std::string targetName = target->GetName();
    std::string derivName = this->GetDerivationName(targetName);
    std::string installDerivName = derivName + "_install";
    
    nixFileStream << "  \"" << targetName << "_install\" = " << installDerivName << ";\n";
  }
}

void cmGlobalNixGenerator::WriteExternalHeaderDerivations(
  cmGeneratedFileStream& nixFileStream)
{
  std::lock_guard<std::mutex> lock(this->ExternalHeaderMutex);
  
  if (this->ExternalHeaderDerivations.empty()) {
    return;
  }
  
  cmNixWriter writer(nixFileStream);
  writer.WriteComment("External header collection derivations");
  
  for (const auto& [sourceDir, headerInfo] : this->ExternalHeaderDerivations) {
    nixFileStream << "  " << headerInfo.DerivationName << " = stdenv.mkDerivation {\n";
    writer.WriteAttribute("name", "external-headers-" + 
                         cmSystemTools::GetFilenameName(sourceDir));
    
    // Create a composite source that copies all headers
    nixFileStream << "    postUnpack = ''\n";
    
    // Create directory structure for headers
    std::set<std::string> createdDirs;
    for (const std::string& header : headerInfo.Headers) {
      // Get relative path from source directory
      std::string relPath = cmSystemTools::RelativePath(sourceDir, header);
      std::string headerDir = cmSystemTools::GetFilenamePath(relPath);
      if (!headerDir.empty() && createdDirs.find(headerDir) == createdDirs.end()) {
        nixFileStream << "      mkdir -p $out/" << headerDir << "\n";
        createdDirs.insert(headerDir);
      }
    }
    
    // Copy all headers
    for (const std::string& header : headerInfo.Headers) {
      if (cmSystemTools::FileExists(header)) {
        std::string normalizedPath = cmSystemTools::CollapseFullPath(header);
        std::string relPath = cmSystemTools::RelativePath(sourceDir, header);
        nixFileStream << "      cp -L ${builtins.path { path = \"" 
                     << normalizedPath << "\"; }} $out/" << relPath 
                     << " 2>/dev/null || true\n";
      }
    }
    
    nixFileStream << "    '';\n";
    writer.WriteAttribute("dontUnpack", "true");
    writer.WriteAttribute("dontBuild", "true");
    writer.WriteAttribute("dontInstall", "true");
    writer.WriteAttribute("dontFixup", "true");
    nixFileStream << "  };\n\n";
  }
}

std::string cmGlobalNixGenerator::GetOrCreateHeaderDerivation(
  const std::string& sourceDir, 
  const std::vector<std::string>& headers)
{
  std::lock_guard<std::mutex> lock(this->ExternalHeaderMutex);
  
  // Check if we already have a header derivation for this directory
  auto it = this->ExternalHeaderDerivations.find(sourceDir);
  if (it != this->ExternalHeaderDerivations.end()) {
    // Add any new headers to the existing derivation
    for (const std::string& header : headers) {
      it->second.Headers.insert(header);
    }
    return it->second.DerivationName;
  }
  
  // Create a new header derivation
  HeaderDerivationInfo info;
  info.SourceDirectory = sourceDir;
  info.DerivationName = this->GetDerivationName("external_headers_" + 
                        cmSystemTools::GetFilenameName(sourceDir));
  
  for (const std::string& header : headers) {
    info.Headers.insert(header);
  }
  
  this->ExternalHeaderDerivations[sourceDir] = info;
  return info.DerivationName;
}

void cmGlobalNixGenerator::CollectInstallTargets()
{
  std::lock_guard<std::mutex> lock(this->InstallTargetsMutex);
  this->InstallTargets.clear();
  
  for (auto const& lg : this->LocalGenerators) {
    for (auto const& target : lg->GetGeneratorTargets()) {
      if (target->GetType() == cmStateEnums::EXECUTABLE ||
          target->GetType() == cmStateEnums::STATIC_LIBRARY ||
          target->GetType() == cmStateEnums::SHARED_LIBRARY ||
          target->GetType() == cmStateEnums::MODULE_LIBRARY ||
          target->GetType() == cmStateEnums::OBJECT_LIBRARY) {
        if(!target->Target->GetInstallGenerators().empty()) {
          this->InstallTargets.push_back(target.get());
        }
      }
    }
  }
}

void cmGlobalNixGenerator::WriteInstallRules(cmGeneratedFileStream& nixFileStream)
{
  std::lock_guard<std::mutex> lock(this->InstallTargetsMutex);
  if (this->InstallTargets.empty()) {
    return;
  }
  
  nixFileStream << "\n  # Install derivations\n";
  
  for (cmGeneratorTarget* target : this->InstallTargets) {
    std::string targetName = target->GetName();
    std::string derivName = this->GetDerivationName(targetName);
    std::string installDerivName = derivName + "_install";
    
    nixFileStream << "  " << installDerivName << " = stdenv.mkDerivation {\n";
    nixFileStream << "    name = \"" << targetName << "-install\";\n";
    nixFileStream << "    src = " << derivName << ";\n";
    nixFileStream << "    dontUnpack = true;\n";
    nixFileStream << "    dontBuild = true;\n";
    nixFileStream << "    dontConfigure = true;\n";
    nixFileStream << "    installPhase = ''\n";

    // Get install destination, with error handling for missing install generators
    std::string dest;
    const auto& installGens = target->Target->GetInstallGenerators();
    if (installGens.empty()) {
      // No install rules defined - use default destinations
      if (target->GetType() == cmStateEnums::EXECUTABLE) {
        dest = "bin";
      } else if (target->GetType() == cmStateEnums::SHARED_LIBRARY || 
                 target->GetType() == cmStateEnums::STATIC_LIBRARY) {
        dest = "lib";
      } else {
        dest = "share";
      }
    } else {
      dest = installGens[0]->GetDestination(this->GetBuildConfiguration(target));
    }
    
    std::string escapedDest = cmOutputConverter::EscapeForShell(dest, cmOutputConverter::Shell_Flag_IsUnix);
    std::string escapedTargetName = cmOutputConverter::EscapeForShell(targetName, cmOutputConverter::Shell_Flag_IsUnix);
    
    nixFileStream << "      mkdir -p $out/" << escapedDest << "\n";
    
    // Determine installation destination based on target type
    if (target->GetType() == cmStateEnums::EXECUTABLE) {
      nixFileStream << "      cp $src $out/" << escapedDest << "/" << escapedTargetName << "\n";
    } else if (target->GetType() == cmStateEnums::SHARED_LIBRARY) {
      nixFileStream << "      cp -r $src/* $out/" << escapedDest << "/ 2>/dev/null || true\n";
    } else if (target->GetType() == cmStateEnums::STATIC_LIBRARY) {
      std::string libName = this->GetLibraryPrefix() + targetName + this->GetStaticLibraryExtension();
      std::string escapedLibName = cmOutputConverter::EscapeForShell(libName, cmOutputConverter::Shell_Flag_IsUnix);
      nixFileStream << "      cp $src $out/" << escapedDest << "/" << escapedLibName << "\n";
    }
    
    nixFileStream << "    '';\n";
    nixFileStream << "  };\n\n";
  }
}

// Dependency graph implementation
void cmGlobalNixGenerator::BuildDependencyGraph() {
  ProfileTimer timer(this, "BuildDependencyGraph");
  
  // Clear any existing graph
  this->DependencyGraph.Clear();
  
  // Add all targets to the graph
  for (const auto& lg : this->LocalGenerators) {
    for (const auto& target : lg->GetGeneratorTargets()) {
      this->DependencyGraph.AddTarget(target->GetName(), target.get());
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
            this->DependencyGraph.AddDependency(target->GetName(), item.Target->GetName());
          }
        }
      }
    }
  }
}

void cmGlobalNixGenerator::cmNixDependencyGraph::AddTarget(const std::string& name, cmGeneratorTarget* target) {
  cmNixDependencyNode node;
  node.targetName = name;
  node.type = target->GetType();
  nodes[name] = node;
}

void cmGlobalNixGenerator::cmNixDependencyGraph::AddDependency(const std::string& from, const std::string& to) {
  // Add 'to' as a direct dependency of 'from'
  auto it = nodes.find(from);
  if (it != nodes.end()) {
    it->second.directDependencies.push_back(to);
    // Clear cached transitive dependencies since graph has changed
    it->second.transitiveDepsComputed = false;
    it->second.transitiveDependencies.clear();
    
    // CRITICAL FIX: Clear cache for all nodes that might depend on 'from'
    // When 'from' gets a new dependency, any node that transitively depends on 'from'
    // needs its cache invalidated as well
    for (auto& nodePair : nodes) {
      if (nodePair.second.transitiveDepsComputed) {
        // If this node's transitive deps include 'from', clear its cache
        if (nodePair.second.transitiveDependencies.count(from) > 0 ||
            nodePair.first == from) {
          nodePair.second.transitiveDepsComputed = false;
          nodePair.second.transitiveDependencies.clear();
        }
      }
    }
  }
}

std::set<std::string> cmGlobalNixGenerator::cmNixDependencyGraph::GetTransitiveSharedLibraries(const std::string& target) const {
  auto it = nodes.find(target);
  if (it == nodes.end()) {
    return {};
  }
  
  auto& node = it->second;
  
  // Return cached result if available
  if (node.transitiveDepsComputed) {
    return node.transitiveDependencies;
  }
  
  // Compute transitive dependencies using DFS
  std::set<std::string> visited;
  std::set<std::string> result;
  std::vector<std::string> stack;
  
  stack.push_back(target);
  
  while (!stack.empty()) {
    std::string current = stack.back();
    stack.pop_back();
    
    if (visited.count(current)) continue;
    visited.insert(current);
    
    auto currentIt = nodes.find(current);
    if (currentIt == nodes.end()) continue;
    
    auto& currentNode = currentIt->second;
    
    // If this is a shared or module library (and not the starting target), include it
    if (current != target && 
        (currentNode.type == cmStateEnums::SHARED_LIBRARY || 
         currentNode.type == cmStateEnums::MODULE_LIBRARY)) {
      result.insert(current);
    }
    
    // Add direct dependencies to stack
    for (const auto& dep : currentNode.directDependencies) {
      if (!visited.count(dep)) {
        stack.push_back(dep);
      }
    }
  }
  
  // Cache the result
  node.transitiveDependencies = result;
  node.transitiveDepsComputed = true;
  
  return result;
}

bool cmGlobalNixGenerator::cmNixDependencyGraph::HasCircularDependency() const {
  // Simple cycle detection using DFS
  std::set<std::string> visited;
  std::set<std::string> recursionStack;
  
  for (const auto& pair : nodes) {
    if (visited.find(pair.first) == visited.end()) {
      std::function<bool(const std::string&)> dfs = [&](const std::string& node) -> bool {
        visited.insert(node);
        recursionStack.insert(node);
        
        auto it = nodes.find(node);
        if (it != nodes.end()) {
          for (const auto& dep : it->second.directDependencies) {
            if (recursionStack.count(dep)) {
              return true; // Back edge found - cycle detected
            }
            if (visited.find(dep) == visited.end() && dfs(dep)) {
              return true;
            }
          }
        }
        
        recursionStack.erase(node);
        return false;
      };
      
      if (dfs(pair.first)) {
        return true;
      }
    }
  }
  
  return false;
}

void cmGlobalNixGenerator::cmNixDependencyGraph::Clear() {
  nodes.clear();
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

void cmGlobalNixGenerator::ProcessHeaderDependencies(
  const std::vector<std::string>& headers,
  const std::string& buildDir,
  const std::string& srcDir,
  std::vector<std::string>& existingFiles,
  std::vector<std::string>& generatedFiles,
  std::vector<std::string>& configTimeGeneratedFiles)
{
  for (const auto& dep : headers) {
    // Get full path for checking
    std::string fullPath = dep;
    if (!cmSystemTools::FileIsFullPath(dep)) {
      fullPath = this->GetCMakeInstance()->GetHomeDirectory();
      fullPath += "/";
      fullPath += dep;
    }
    
    if (this->GetCMakeInstance()->GetDebugOutput()) {
      std::cerr << "[NIX-DEBUG] Processing header dependency: " << dep 
                << " (full: " << fullPath << ")" << std::endl;
      std::cerr << "[NIX-DEBUG] File exists: " << cmSystemTools::FileExists(fullPath) << std::endl;
    }
    
    // Check if file is in build directory
    bool isInBuildDir = (fullPath.find(buildDir) == 0);
    bool isInSourceDir = (fullPath.find(srcDir) == 0);
    
    // Only consider it a config-time generated file if:
    // 1. It's in the build directory
    // 2. It's NOT also in the source directory (in-source builds)
    // 3. OR the build dir and source dir are different and file is only in build dir
    bool isConfigTimeGenerated = isInBuildDir && 
      (buildDir != srcDir || !isInSourceDir);
    
    // Convert to appropriate relative path
    std::string relDep;
    if (isInBuildDir && buildDir != srcDir) {
      // For build directory files, make path relative to source directory
      // since the fileset will be rooted at the source directory
      relDep = cmSystemTools::RelativePath(srcDir, fullPath);
    } else if (cmSystemTools::FileIsFullPath(dep)) {
      // For source directory files, make path relative to source dir
      relDep = cmSystemTools::RelativePath(
        this->GetCMakeInstance()->GetHomeDirectory(), dep);
    } else {
      relDep = dep;
    }
    
    if (this->GetCMakeInstance()->GetDebugOutput()) {
      std::cerr << "[NIX-DEBUG] Relative dependency path: " << relDep << std::endl;
    }
    
    // Add if it's a valid relative path
    if (!relDep.empty()) {
      if (cmSystemTools::FileExists(fullPath)) {
        // Check if it's a configuration-time generated file (exists in build dir)
        if (isConfigTimeGenerated) {
          // This is a configuration-time generated file (like Zephyr's autoconf.h)
          configTimeGeneratedFiles.push_back(fullPath);
          if (this->GetCMakeInstance()->GetDebugOutput()) {
            std::cerr << "[NIX-DEBUG] Added config-time generated header: " << fullPath << std::endl;
          }
        } else {
          existingFiles.push_back(relDep);
          if (this->GetCMakeInstance()->GetDebugOutput()) {
            std::cerr << "[NIX-DEBUG] Added existing header to fileset: " << relDep << std::endl;
          }
        }
      } else {
        // Header might be generated during build (custom commands)
        // Check if this header is a custom command output
        auto customIt = this->CustomCommandOutputs.find(fullPath);
        if (customIt != this->CustomCommandOutputs.end()) {
          // This header is generated by a custom command
          generatedFiles.push_back(relDep);
          if (this->GetCMakeInstance()->GetDebugOutput()) {
            std::cerr << "[NIX-DEBUG] Added custom command generated header: " << relDep 
                      << " (from derivation: " << customIt->second << ")" << std::endl;
          }
        } else {
          // Regular generated file (not custom command)
          generatedFiles.push_back(relDep);
          if (this->GetCMakeInstance()->GetDebugOutput()) {
            std::cerr << "[NIX-DEBUG] Added build-time generated header: " << relDep 
                      << " (full: " << fullPath << ")" << std::endl;
          }
        }
      }
    }
  }
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
      inFile.close();
      
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
  
  if (this->GetCMakeInstance()->GetDebugOutput()) {
    std::cerr << "[NIX-DEBUG] Language: " << lang
              << ", Compiler package: " << compilerPkg 
              << (needs32Bit ? " (32-bit)" : "") << std::endl;
  }
  
  // Get external library dependencies
  std::vector<std::string> libraryDeps = this->GetCachedLibraryDependencies(target, config);
  this->ProcessLibraryDependenciesForBuildInputs(libraryDeps, buildInputs, projectSourceRelPath);
  
  // Check if this source file is generated by a custom command
  auto it = this->CustomCommandOutputs.find(sourceFile);
  if (it != this->CustomCommandOutputs.end()) {
    buildInputs.push_back(it->second);
    if (this->GetCMakeInstance()->GetDebugOutput()) {
      std::cerr << "[NIX-DEBUG] Found custom command dependency for " << sourceFile 
                << " -> " << it->second << std::endl;
    }
  } else {
    if (this->GetCMakeInstance()->GetDebugOutput()) {
      std::cerr << "[NIX-DEBUG] No custom command found for " << sourceFile << std::endl;
      std::cerr << "[NIX-DEBUG] Available custom command outputs:" << std::endl;
      for (const auto& kv : this->CustomCommandOutputs) {
        std::cerr << "[NIX-DEBUG]   " << kv.first << " -> " << kv.second << std::endl;
      }
    }
  }
  
  // Check if any header dependencies are generated by custom commands
  auto targetGen = cmNixTargetGenerator::New(target);
  std::vector<std::string> headers = targetGen->GetSourceDependencies(source);
  
  if (this->GetCMakeInstance()->GetDebugOutput() && !headers.empty()) {
    std::cerr << "[NIX-DEBUG] Checking header dependencies for " << sourceFile << std::endl;
    for (const auto& header : headers) {
      std::cerr << "[NIX-DEBUG]   Header: " << header << std::endl;
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
      pathsToCheck.push_back(this->GetCMakeInstance()->GetHomeOutputDirectory() + "/" + header);
      // Try with the header as-is (relative path)
      pathsToCheck.push_back(header);
    }
    
    // Also check for headers that might include "zephyr/" prefix
    if (header.find("zephyr/syscall_list.h") != std::string::npos ||
        header.find("syscall_list.h") != std::string::npos) {
      std::string buildDir = this->GetCMakeInstance()->GetHomeOutputDirectory();
      pathsToCheck.push_back(buildDir + "/zephyr/include/generated/zephyr/syscall_list.h");
      pathsToCheck.push_back(buildDir + "/include/generated/zephyr/syscall_list.h");
    }
    
    // Check each possible path
    bool found = false;
    for (const auto& pathToCheck : pathsToCheck) {
      auto customIt = this->CustomCommandOutputs.find(pathToCheck);
      if (customIt != this->CustomCommandOutputs.end()) {
        // Only add if not already in buildInputs
        if (std::find(buildInputs.begin(), buildInputs.end(), customIt->second) == buildInputs.end()) {
          buildInputs.push_back(customIt->second);
          if (this->GetCMakeInstance()->GetDebugOutput()) {
            std::cerr << "[NIX-DEBUG] Found custom command generated header dependency: " 
                      << header << " (resolved to " << pathToCheck << ") -> " << customIt->second << std::endl;
          }
          found = true;
          break;
        }
      }
    }
    
    if (this->GetCMakeInstance()->GetDebugOutput() && !found) {
      std::cerr << "[NIX-DEBUG] Header " << header << " not found in custom command outputs" << std::endl;
      std::cerr << "[NIX-DEBUG] Checked paths:" << std::endl;
      for (const auto& path : pathsToCheck) {
        std::cerr << "[NIX-DEBUG]   - " << path << std::endl;
      }
    }
  }
  
  // Check if this source file has an external header derivation dependency
  {
    std::lock_guard<std::mutex> lock(this->ExternalHeaderMutex);
    auto headerIt = this->SourceToHeaderDerivation.find(sourceFile);
    if (headerIt != this->SourceToHeaderDerivation.end() && !headerIt->second.empty()) {
      buildInputs.push_back(headerIt->second);
      if (this->GetCMakeInstance()->GetDebugOutput()) {
        std::cerr << "[NIX-DEBUG] Found header derivation dependency for " << sourceFile 
                  << " -> " << headerIt->second << std::endl;
      }
    }
  }
  
  return buildInputs;
}

std::vector<std::string> cmGlobalNixGenerator::FilterProjectHeaders(
  const std::vector<std::string>& headers)
{
  std::vector<std::string> projectHeaders;
  
  for (const std::string& header : headers) {
    // Skip system headers
    if (this->IsSystemPath(header)) {
      continue;
    }
    // Skip absolute paths outside project (system headers)
    if (cmSystemTools::FileIsFullPath(header)) {
      // Check if it's in the project or build directory
      std::string projectDir = this->GetCMakeInstance()->GetHomeDirectory();
      std::string buildDir = this->GetCMakeInstance()->GetHomeOutputDirectory();
      if (!cmSystemTools::IsSubDirectory(header, projectDir) &&
          !cmSystemTools::IsSubDirectory(header, buildDir)) {
        continue;
      }
    }
    // Skip configuration-time generated files that are already in composite source
    // These would have paths like "build/zephyr/include/..." when build != source
    if (header.find("build/") == 0 || header.find("./build/") == 0) {
      continue;
    }
    projectHeaders.push_back(header);
  }
  
  return projectHeaders;
}

bool cmGlobalNixGenerator::IsSystemPath(const std::string& path) const
{
  // Check for CMAKE_NIX_SYSTEM_PATH_PREFIXES variable
  cmake* cm = this->GetCMakeInstance();
  cmValue systemPaths = cm->GetCacheDefinition("CMAKE_NIX_SYSTEM_PATH_PREFIXES");
  
  if (systemPaths && !systemPaths->empty()) {
    // User-defined system path prefixes (semicolon-separated)
    std::vector<std::string> prefixes;
    cmExpandList(*systemPaths, prefixes);
    for (const auto& prefix : prefixes) {
      if (cmSystemTools::IsSubDirectory(path, prefix)) {
        return true;
      }
    }
  } else {
    // Default system paths
    static const std::vector<std::string> defaultSystemPaths = {
      "/usr",
      "/nix/store",
      "/opt",
      "/usr/local",
      "/System",  // macOS
      "/Library"  // macOS
    };
    
    // Also consider CMake's own modules directory as a system path
    std::string cmakeRoot = cmSystemTools::GetCMakeRoot();
    if (!cmakeRoot.empty() && cmSystemTools::IsSubDirectory(path, cmakeRoot)) {
      return true;
    }
    
    for (const auto& systemPath : defaultSystemPaths) {
      if (cmSystemTools::IsSubDirectory(path, systemPath)) {
        return true;
      }
    }
  }
  
  return false;
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
  // Create local copies for dependency processing
  std::vector<CustomCommandInfo> localCustomCommands = this->CustomCommands;
  std::map<std::string, std::string> localCustomCommandOutputs = this->CustomCommandOutputs;
  
  // Second pass: Write custom commands in dependency order
  std::set<std::string> written;
  std::vector<size_t> orderedCommands; // Store indices instead of pointers
  
  // Simple topological sort using Kahn's algorithm
  std::map<std::string, std::vector<size_t>> dependents; // Store indices
  std::map<std::string, int> inDegree;
  
  // Build dependency graph using local copies (thread-safe)
  for (const auto& info : localCustomCommands) {
    inDegree[info.DerivationName] = 0;
  }
  
  // PERFORMANCE FIX: Create map for O(1) lookup instead of O(n) search
  std::map<std::string, size_t> derivNameToIndex;
  for (size_t i = 0; i < localCustomCommands.size(); ++i) {
    derivNameToIndex[localCustomCommands[i].DerivationName] = i;
  }
  
  // Build dependency graph using indices - now O(n*m) instead of O(n²*m)
  for (size_t i = 0; i < localCustomCommands.size(); ++i) {
    const auto& info = localCustomCommands[i];
    for (const std::string& dep : info.Depends) {
      auto depIt = localCustomCommandOutputs.find(dep);
      if (depIt != localCustomCommandOutputs.end()) {
        // Use the map for O(1) lookup
        auto indexIt = derivNameToIndex.find(depIt->second);
        if (indexIt != derivNameToIndex.end()) {
          dependents[depIt->second].push_back(i);
          inDegree[info.DerivationName]++;
          break;
        }
      }
    }
  }
  
  // Find nodes with no dependencies
  std::queue<size_t> q;
  for (size_t i = 0; i < localCustomCommands.size(); ++i) {
    if (inDegree[localCustomCommands[i].DerivationName] == 0) {
      q.push(i);
    }
  }
  
  // Process in dependency order
  while (!q.empty()) {
    size_t currentIdx = q.front();
    q.pop();
    orderedCommands.push_back(currentIdx);
    
    // Reduce in-degree for dependents
    const std::string& currentName = localCustomCommands[currentIdx].DerivationName;
    for (size_t dependentIdx : dependents[currentName]) {
      if (--inDegree[localCustomCommands[dependentIdx].DerivationName] == 0) {
        q.push(dependentIdx);
      }
    }
  }
  
  // Check for cycles - if not all commands were processed, there's a cycle
  if (orderedCommands.size() != localCustomCommands.size()) {
    std::ostringstream msg;
    msg << "CMake Error: Cyclic dependency detected in custom commands. ";
    msg << "Processed " << orderedCommands.size() << " of " 
        << localCustomCommands.size() << " commands.\n\n";
    
    // Debug output
    if (this->GetCMakeInstance()->GetDebugOutput()) {
      std::cerr << "[NIX-DEBUG] Total custom commands: " << localCustomCommands.size() << std::endl;
      std::cerr << "[NIX-DEBUG] Ordered commands: " << orderedCommands.size() << std::endl;
      for (size_t i = 0; i < localCustomCommands.size(); ++i) {
        std::cerr << "[NIX-DEBUG] Command " << i << ": " << localCustomCommands[i].DerivationName << std::endl;
      }
    }
    
    // Find which commands weren't processed (part of cycles)
    std::set<std::string> processedNames;
    for (size_t idx : orderedCommands) {
      processedNames.insert(localCustomCommands[idx].DerivationName);
    }
    
    std::vector<size_t> cyclicCommands;
    for (size_t i = 0; i < localCustomCommands.size(); ++i) {
      if (processedNames.find(localCustomCommands[i].DerivationName) == processedNames.end()) {
        cyclicCommands.push_back(i);
      }
    }
    
    // More debug output  
    if (this->GetCMakeInstance()->GetDebugOutput()) {
      std::cerr << "[NIX-DEBUG] Unprocessed commands: " << cyclicCommands.size() << std::endl;
      for (size_t idx : cyclicCommands) {
        const auto& cmd = localCustomCommands[idx];
        std::cerr << "[NIX-DEBUG] Unprocessed: " << cmd.DerivationName << " (indegree=" << inDegree[cmd.DerivationName] << ")" << std::endl;
      }
    }
    
    msg << "Commands involved in circular dependencies (" << cyclicCommands.size() << " commands):\n";
    
    // Enhanced reporting with more context
    for (size_t idx : cyclicCommands) {
      const auto& info = localCustomCommands[idx];
      msg << "  • " << info.DerivationName << "\n";
      msg << "    Working directory: " << info.LocalGen->GetCurrentBinaryDirectory() << "\n";
      
      // Show the command itself (first few words)
      if (info.Command && !info.Command->GetCommandLines().empty()) {
        const auto& cmdLine = info.Command->GetCommandLines()[0];
        if (!cmdLine.empty()) {
          std::string cmdStr = cmdLine[0];
          if (cmdLine.size() > 1) {
            cmdStr += " " + cmdLine[1];
          }
          if (cmdLine.size() > 2) {
            cmdStr += " ...";
          }
          msg << "    Command: " << cmdStr << "\n";
        }
      }
      
      // Show outputs this command produces
      msg << "    Outputs: ";
      if (info.Outputs.empty()) {
        msg << "(none)";
      } else {
        for (size_t i = 0; i < info.Outputs.size(); ++i) {
          if (i > 0) msg << ", ";
          msg << cmSystemTools::GetFilenameName(info.Outputs[i]);
        }
      }
      msg << "\n";
      
      // Show dependencies this command has
      msg << "    Depends on: ";
      if (info.Depends.empty()) {
        msg << "(none)";
      } else {
        bool first = true;
        for (const std::string& dep : info.Depends) {
          if (!first) msg << ", ";
          first = false;
          
          auto depIt = this->CustomCommandOutputs.find(dep);
          if (depIt != this->CustomCommandOutputs.end()) {
            msg << depIt->second << " (via " << cmSystemTools::GetFilenameName(dep) << ")";
          } else {
            msg << cmSystemTools::GetFilenameName(dep);
          }
        }
      }
      msg << "\n\n";
    }
    
    // Try to detect and report specific cycles
    msg << "Cycle Analysis:\n";
    std::function<bool(const std::string&, std::set<std::string>&, std::vector<std::string>&, int)> findCycle;
    findCycle = [&](const std::string& current, std::set<std::string>& visited, std::vector<std::string>& path, int depth) -> bool {
      // Prevent stack overflow with depth limit
      if (depth > MAX_CYCLE_DETECTION_DEPTH) {
        this->GetCMakeInstance()->IssueMessage(MessageType::WARNING, "Cycle detection depth limit exceeded at: " + current);
        return false;
      }
      if (visited.count(current)) {
        // Found a cycle - report it
        auto cycleStart = std::find(path.begin(), path.end(), current);
        if (cycleStart != path.end()) {
          msg << "  Detected cycle: ";
          bool first = true;
          for (auto it = cycleStart; it != path.end(); ++it) {
            if (!first) msg << " → ";
            first = false;
            msg << *it;
          }
          msg << " → " << current << "\n";
          return true;
        }
      }
      
      visited.insert(current);
      path.push_back(current);
      
      // Follow dependencies
      for (size_t idx : cyclicCommands) {
        const auto& info = localCustomCommands[idx];
        if (info.DerivationName == current) {
          for (const std::string& dep : info.Depends) {
            auto depIt = localCustomCommandOutputs.find(dep);
            if (depIt != localCustomCommandOutputs.end()) {
              if (findCycle(depIt->second, visited, path, depth + 1)) {
                return true;
              }
            }
          }
          break;
        }
      }
      
      path.pop_back();
      visited.erase(current);
      return false;
    };
    
    std::set<std::string> visited;
    std::vector<std::string> path;
    bool foundCycle = false;
    for (size_t idx : cyclicCommands) {
      if (!foundCycle) {
        foundCycle = findCycle(localCustomCommands[idx].DerivationName, visited, path, 0);
      }
    }
    
    if (!foundCycle) {
      msg << "  Unable to trace specific cycle (complex interdependencies)\n";
    }
    
    msg << "\nWORKAROUND FOR COMPLEX BUILD SYSTEMS:\n";
    msg << "The Nix generator has detected circular dependencies in custom commands, which\n";
    msg << "typically occurs with complex build systems like Zephyr, Linux kernel, etc.\n";
    msg << "\n";
    msg << "To work around this issue, you can:\n";
    msg << "1. Use the Ninja generator instead: cmake -GNinja -DBOARD=native_sim/native/64 .\n";
    msg << "2. Or set CMAKE_NIX_IGNORE_CIRCULAR_DEPS=ON to bypass this check (experimental)\n";
    msg << "\n";
    msg << "GENERAL SUGGESTIONS:\n";
    msg << "• Check if custom commands have correct INPUT/OUTPUT dependencies\n";
    msg << "• Verify that generated files are not both input and output of different commands\n";
    msg << "• Consider breaking complex dependencies into separate steps\n";
    msg << "• Use add_dependencies() to establish explicit ordering when needed\n";
    
    // Check if user wants to bypass this check
    cmValue ignoreCircular = this->GetCMakeInstance()->GetCacheDefinition("CMAKE_NIX_IGNORE_CIRCULAR_DEPS");
    if (ignoreCircular && (*ignoreCircular == "ON" || *ignoreCircular == "1" || *ignoreCircular == "YES" || *ignoreCircular == "TRUE")) {
      this->GetCMakeInstance()->IssueMessage(MessageType::WARNING, 
        "Circular dependencies detected, but proceeding due to CMAKE_NIX_IGNORE_CIRCULAR_DEPS=ON\n"
        "This may result in incorrect build order and build failures.\n"
        + std::to_string(this->CustomCommands.size() - orderedCommands.size()) + 
        " commands have circular dependencies but will be processed anyway.");
      
      // Process all commands regardless of dependencies - append the unprocessed ones to orderedCommands
      for (size_t i = 0; i < this->CustomCommands.size(); ++i) {
        bool found = false;
        for (size_t idx : orderedCommands) {
          if (idx == i) {
            found = true;
            break;
          }
        }
        if (!found) {
          orderedCommands.push_back(i);
        }
      }
      
      if (this->GetCMakeInstance()->GetDebugOutput()) {
        std::cerr << "[NIX-DEBUG] Now processing all " << orderedCommands.size() << " custom commands." << std::endl;
        std::cerr << "[NIX-DEBUG] About to write custom commands to Nix file..." << std::endl;
      }
    } else {
      this->GetCMakeInstance()->IssueMessage(MessageType::FATAL_ERROR, msg.str());
      return;
    }
  }
  
  // Write commands in order
  if (this->GetCMakeInstance()->GetDebugOutput()) {
    std::cerr << "[NIX-DEBUG] Writing " << orderedCommands.size() << " custom commands" << std::endl;
    std::cerr << "[NIX-DEBUG] CustomCommandOutputs has " << this->CustomCommandOutputs.size() << " entries" << std::endl;
    std::cerr << "[NIX-DEBUG] ObjectFileOutputs has " << this->ObjectFileOutputs.size() << " entries" << std::endl;
  }
  for (size_t idx : orderedCommands) {
    const CustomCommandInfo* info = &localCustomCommands[idx];
    try {
      std::string config = "Release";
      const auto& makefiles = this->GetCMakeInstance()->GetGlobalGenerator()->GetMakefiles();
      if (!makefiles.empty()) {
        config = makefiles[0]->GetSafeDefinition("CMAKE_BUILD_TYPE");
        if (config.empty()) {
          config = "Release";
        }
      }
      cmNixCustomCommandGenerator ccg(info->Command, info->LocalGen, config, &this->CustomCommandOutputs, &this->ObjectFileOutputs);
      ccg.Generate(nixFileStream);
    } catch (const std::bad_alloc& e) {
      std::ostringstream msg;
      msg << "Out of memory writing custom command " << info->DerivationName;
      this->GetCMakeInstance()->IssueMessage(MessageType::WARNING, msg.str());
    } catch (const std::system_error& e) {
      std::ostringstream msg;
      msg << "System error writing custom command " << info->DerivationName 
          << ": " << e.what() << " (code: " << e.code() << ")";
      this->GetCMakeInstance()->IssueMessage(MessageType::WARNING, msg.str());
    } catch (const std::runtime_error& e) {
      std::ostringstream msg;
      msg << "Runtime error writing custom command " << info->DerivationName << ": " << e.what();
      this->GetCMakeInstance()->IssueMessage(MessageType::WARNING, msg.str());
    } catch (const std::exception& e) {
      std::ostringstream msg;
      msg << "Exception writing custom command " << info->DerivationName << ": " << e.what();
      this->GetCMakeInstance()->IssueMessage(MessageType::WARNING, msg.str());
    }
  }
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
            std::string pkgFileName = this->GetCMakeInstance()->GetHomeOutputDirectory() + 
                                    "/pkg_" + pkg.first + ".nix";
            
            if (this->GetCMakeInstance()->GetDebugOutput()) {
              std::cerr << "[NIX-DEBUG] Found " << pkg.first << " in line: " << line << std::endl;
              std::cerr << "[NIX-DEBUG] Would create: " << pkgFileName << std::endl;
            }
            
            if (!cmSystemTools::FileExists(pkgFileName)) {
              std::ofstream pkgFile(pkgFileName);
              pkgFile << "# Skeleton Nix package file for " << pkg.first << "\n";
              pkgFile << "# Edit this file to specify the correct Nix package\n";
              pkgFile << pkg.second << "\n";
              pkgFile.close();
              
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