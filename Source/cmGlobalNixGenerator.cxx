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

#include "cmGeneratedFileStream.h"
#include "cmGeneratorTarget.h"
#include "cmLocalNixGenerator.h"
#include "cmMakefile.h"
#include "cmSourceFile.h"
#include "cmSystemTools.h"
#include "cmake.h"
#include "cmNixTargetGenerator.h"
#include "cmNixCustomCommandGenerator.h"
#include "cmInstallGenerator.h"
#include "cmInstallTargetGenerator.h"
#include "cmCustomCommand.h"
#include "cmListFileCache.h"
#include "cmValue.h"
#include "cmState.h"

// String constants for performance optimization
const std::string cmGlobalNixGenerator::DefaultConfig = "Release";
const std::string cmGlobalNixGenerator::CLanguage = "C";
const std::string cmGlobalNixGenerator::CXXLanguage = "CXX";
const std::string cmGlobalNixGenerator::GccCompiler = "gcc";
const std::string cmGlobalNixGenerator::ClangCompiler = "clang";

cmGlobalNixGenerator::cmGlobalNixGenerator(cmake* cm)
  : cmGlobalCommonGenerator(cm)
{
  // Set the make program file
  this->FindMakeProgramFile = "CMakeNixFindMake.cmake";
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
  std::cerr << "[NIX-TRACE] " << __FILE__ << ":" << __LINE__ << " Generate() started" << std::endl;
  
  // First call the parent Generate to set up targets
  this->cmGlobalGenerator::Generate();
  
  std::cerr << "[NIX-TRACE] " << __FILE__ << ":" << __LINE__ << " Parent Generate() completed" << std::endl;
  
  // Build dependency graph for transitive dependency resolution
  this->BuildDependencyGraph();
  
  // Generate our Nix output
  this->WriteNixFile();
  
  std::cerr << "[NIX-TRACE] " << __FILE__ << ":" << __LINE__ << " Generate() completed" << std::endl;
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
  
  std::cerr << "[NIX-TRACE] " << __FILE__ << ":" << __LINE__ 
            << " GenerateBuildCommand() called for projectDir: " << projectDir
            << " isTryCompile: " << (isTryCompile ? "true" : "false")
            << " targetNames: ";
  for (const auto& t : targetNames) {
    std::cerr << t << " ";
  }
  std::cerr << std::endl;
  
  GeneratedMakeCommand makeCommand;
  
  // For Nix generator, we use nix-build as the build program
  makeCommand.Add(this->SelectMakeProgram(makeProgram, "nix-build"));
  
  // For try_compile, look for default.nix in the scratch directory without suffix
  if (isTryCompile) {
    // Extract the base scratch directory (remove numeric suffix if present)
    std::string scratchDir = projectDir;
    size_t underscorePos = scratchDir.find_last_of('_');
    if (underscorePos != std::string::npos) {
      // Check if everything after the underscore is numeric
      std::string suffix = scratchDir.substr(underscorePos + 1);
      bool isNumeric = !suffix.empty() && std::all_of(suffix.begin(), suffix.end(), ::isdigit);
      if (isNumeric) {
        scratchDir = scratchDir.substr(0, underscorePos);
      }
    }
    makeCommand.Add(scratchDir + "/default.nix");
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
    std::cerr << "[NIX-TRACE] " << __FILE__ << ":" << __LINE__ 
              << " Generating try-compile copy commands" << std::endl;
    
    GeneratedMakeCommand copyCommand;
    copyCommand.Add("sh");
    copyCommand.Add("-c");
    
    std::string copyScript = "set -e; ";
    for (auto const& tname : targetNames) {
      if (!tname.empty()) {
        std::cerr << "[NIX-TRACE] " << __FILE__ << ":" << __LINE__ 
                  << " Adding copy command for target: " << tname << std::endl;
        
        // Read the target location file and copy the binary
        copyScript += "if [ -f \"" + tname + "_loc\" ]; then ";
        copyScript += "TARGET_LOCATION=$(cat \"" + tname + "_loc\"); ";
        copyScript += "echo '[NIX-TRACE] Target location: '$TARGET_LOCATION; ";
        copyScript += "if [ -f \"result\" ]; then ";
        copyScript += "STORE_PATH=$(readlink result); ";
        copyScript += "echo '[NIX-TRACE] Store path: '$STORE_PATH; ";
        copyScript += "cp \"$STORE_PATH\" \"$TARGET_LOCATION\" 2>/dev/null || echo '[NIX-TRACE] Copy failed'; ";
        copyScript += "else echo '[NIX-TRACE] No result symlink found'; fi; ";
        copyScript += "else echo '[NIX-TRACE] No location file for " + tname + "'; fi; ";
      }
    }
    copyScript += "true"; // Ensure script always succeeds
    
    copyCommand.Add(copyScript);
    
    return { std::move(makeCommand), std::move(copyCommand) };
  }
  
  return { std::move(makeCommand) };
}

void cmGlobalNixGenerator::WriteNixFile()
{
  std::string nixFile = this->GetCMakeInstance()->GetHomeDirectory();
  nixFile += "/default.nix";
  
  std::cerr << "[NIX-TRACE] " << __FILE__ << ":" << __LINE__ 
            << " WriteNixFile() writing to: " << nixFile << std::endl;
  
  cmGeneratedFileStream nixFileStream(nixFile);
  nixFileStream.SetCopyIfDifferent(true);
  
  if (!nixFileStream) {
    return;
  }

  // Write Nix file header
  nixFileStream << "# Generated by CMake Nix Generator\n"
                << "with import <nixpkgs> {};\n\n"
                << "let\n";

  // First pass: Collect all custom commands
  this->CustomCommands.clear();
  this->CustomCommandOutputs.clear();
  
  for (auto const& lg : this->LocalGenerators) {
    for (auto const& target : lg->GetGeneratorTargets()) {
      std::vector<cmSourceFile*> sources;
      target->GetSourceFiles(sources, "");
      for (cmSourceFile* source : sources) {
        if (cmCustomCommand const* cc = source->GetCustomCommand()) {
          try {
            cmNixCustomCommandGenerator ccg(cc, target->GetLocalGenerator(), this->GetBuildConfiguration(target.get()));
            
            CustomCommandInfo info;
            info.DerivationName = ccg.GetDerivationName();
            info.Outputs = ccg.GetOutputs();
            info.Depends = ccg.GetDepends();
            info.Command = cc;
            info.LocalGen = target->GetLocalGenerator();
            
            this->CustomCommands.push_back(info);
            
            // Populate CustomCommandOutputs map for dependency tracking
            for (const std::string& output : info.Outputs) {
              this->CustomCommandOutputs[output] = info.DerivationName;
            }
          } catch (const std::exception& e) {
            std::cerr << "Exception in custom command processing: " << e.what() << std::endl;
          } catch (...) {
            std::cerr << "Unknown exception in custom command processing" << std::endl;
          }
        }
      }
    }
  }
  
  // Second pass: Write custom commands in dependency order
  std::set<std::string> written;
  std::vector<const CustomCommandInfo*> orderedCommands;
  
  // Simple topological sort using Kahn's algorithm
  std::map<std::string, std::vector<const CustomCommandInfo*>> dependents;
  std::map<std::string, int> inDegree;
  
  // Build dependency graph
  for (const auto& info : this->CustomCommands) {
    inDegree[info.DerivationName] = 0;
  }
  
  for (const auto& info : this->CustomCommands) {
    for (const std::string& dep : info.Depends) {
      auto depIt = this->CustomCommandOutputs.find(dep);
      if (depIt != this->CustomCommandOutputs.end()) {
        dependents[depIt->second].push_back(&info);
        inDegree[info.DerivationName]++;
      }
    }
  }
  
  // Find nodes with no dependencies
  std::queue<const CustomCommandInfo*> q;
  for (const auto& info : this->CustomCommands) {
    if (inDegree[info.DerivationName] == 0) {
      q.push(&info);
    }
  }
  
  // Process in dependency order
  while (!q.empty()) {
    const CustomCommandInfo* current = q.front();
    q.pop();
    orderedCommands.push_back(current);
    
    // Reduce in-degree for dependents
    for (const CustomCommandInfo* dependent : dependents[current->DerivationName]) {
      if (--inDegree[dependent->DerivationName] == 0) {
        q.push(dependent);
      }
    }
  }
  
  // Check for cycles - if not all commands were processed, there's a cycle
  if (orderedCommands.size() != this->CustomCommands.size()) {
    std::ostringstream msg;
    msg << "CMake Error: Cyclic dependency detected in custom commands. ";
    msg << "Processed " << orderedCommands.size() << " of " 
        << this->CustomCommands.size() << " commands.\n";
    msg << "Commands with unresolved dependencies:\n";
    
    // Find which commands weren't processed
    std::set<std::string> processedNames;
    for (const CustomCommandInfo* info : orderedCommands) {
      processedNames.insert(info->DerivationName);
    }
    
    for (const auto& info : this->CustomCommands) {
      if (processedNames.find(info.DerivationName) == processedNames.end()) {
        msg << "  - " << info.DerivationName << " (depends on:";
        for (const std::string& dep : info.Depends) {
          auto depIt = this->CustomCommandOutputs.find(dep);
          if (depIt != this->CustomCommandOutputs.end()) {
            msg << " " << depIt->second;
          }
        }
        msg << ")\n";
      }
    }
    
    this->GetCMakeInstance()->IssueMessage(MessageType::FATAL_ERROR, msg.str());
    return;
  }
  
  // Write commands in order
  for (const CustomCommandInfo* info : orderedCommands) {
    try {
      std::string config = "Release";
      const auto& makefiles = this->GetCMakeInstance()->GetGlobalGenerator()->GetMakefiles();
      if (!makefiles.empty()) {
        config = makefiles[0]->GetSafeDefinition("CMAKE_BUILD_TYPE");
        if (config.empty()) {
          config = "Release";
        }
      }
      cmNixCustomCommandGenerator ccg(info->Command, info->LocalGen, config);
      ccg.Generate(nixFileStream);
    } catch (const std::exception& e) {
      std::cerr << "Exception writing custom command: " << e.what() << std::endl;
    } catch (...) {
      std::cerr << "Unknown exception writing custom command" << std::endl;
    }
  }

  // Collect install targets
  this->CollectInstallTargets();

  // Write per-translation-unit derivations
  this->WritePerTranslationUnitDerivations(nixFileStream);
  
  // Write linking derivations
  this->WriteLinkingDerivations(nixFileStream);
  
  // Write install derivations in the let block  
  this->WriteInstallRules(nixFileStream);
  
  nixFileStream << "in\n{\n";
  
  // Write final target outputs
  for (auto const& lg : this->LocalGenerators) {
    auto const& targets = lg->GetGeneratorTargets();
    for (auto const& target : targets) {
      if (target->GetType() == cmStateEnums::EXECUTABLE ||
          target->GetType() == cmStateEnums::STATIC_LIBRARY ||
          target->GetType() == cmStateEnums::SHARED_LIBRARY ||
          target->GetType() == cmStateEnums::MODULE_LIBRARY) {
        nixFileStream << "  \"" << target->GetName() << "\" = "
                      << this->GetDerivationName(target->GetName()) << ";\n";
      }
    }
  }

  // Write install outputs
  this->WriteInstallOutputs(nixFileStream);
  
  nixFileStream << "}\n";
}

void cmGlobalNixGenerator::WritePerTranslationUnitDerivations(
  cmGeneratedFileStream& nixFileStream)
{
  nixFileStream << "  # Per-translation-unit derivations\n";
  
  for (auto const& lg : this->LocalGenerators) {
    auto const& targets = lg->GetGeneratorTargets();
    for (auto const& target : targets) {
      if (target->GetType() == cmStateEnums::EXECUTABLE ||
          target->GetType() == cmStateEnums::STATIC_LIBRARY ||
          target->GetType() == cmStateEnums::SHARED_LIBRARY ||
          target->GetType() == cmStateEnums::MODULE_LIBRARY ||
          target->GetType() == cmStateEnums::OBJECT_LIBRARY) {
        
        // Get source files for this target
        std::vector<cmSourceFile*> sources;
        target->GetSourceFiles(sources, "");
        
        // Pre-create target generator and cache configuration for efficiency
        auto targetGen = cmNixTargetGenerator::New(target.get());
        std::string config = this->GetBuildConfiguration(target.get());
        
        // Pre-compute and cache library dependencies for this target
        std::pair<cmGeneratorTarget*, std::string> cacheKey = {target.get(), config};
        if (this->LibraryDependencyCache.find(cacheKey) == this->LibraryDependencyCache.end()) {
          this->LibraryDependencyCache[cacheKey] = targetGen->GetTargetLibraryDependencies(config);
        }
        
        for (cmSourceFile* source : sources) {
          std::string const& lang = source->GetLanguage();
          if (lang == "C" || lang == "CXX" || lang == "Fortran" || lang == "CUDA") {
            std::vector<std::string> dependencies = targetGen->GetSourceDependencies(source);
            this->AddObjectDerivation(target->GetName(), this->GetDerivationName(target->GetName(), source->GetFullPath()), source->GetFullPath(), targetGen->GetObjectFileName(source), lang, dependencies);
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
  auto it = this->DerivationNameCache.find(cacheKey);
  if (it != this->DerivationNameCache.end()) {
    return it->second;
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
  
  // Cache the result
  this->DerivationNameCache[cacheKey] = result;
  return result;
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
}

void cmGlobalNixGenerator::WriteObjectDerivation(
  cmGeneratedFileStream& nixFileStream, cmGeneratorTarget* target, 
  cmSourceFile* source)
{
  std::string sourceFile = source->GetFullPath();
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
  
  // Get the local generator for this target
  cmLocalGenerator* lg = target->GetLocalGenerator();
  
  // Get configuration-specific compile flags
  // Use the vector version to properly capture all flags including those from target_compile_options
  std::vector<BT<std::string>> compileFlagsVec = lg->GetTargetCompileFlags(target, config, lang, "");
  std::string compileFlags;
  for (const auto& flag : compileFlagsVec) {
    if (!compileFlags.empty()) compileFlags += " ";
    compileFlags += flag.Value;
  }
  
  // Get configuration-specific preprocessor definitions
  std::set<std::string> defines;
  lg->GetTargetDefines(target, config, lang, defines);
  std::string defineFlags;
  for (const std::string& define : defines) {
    if (!defineFlags.empty()) defineFlags += " ";
    defineFlags += "-D" + define;
  }
  
  // Get include directories from target with proper configuration
  // Use LocalGenerator to properly evaluate generator expressions
  std::vector<std::string> includes;
  lg->GetIncludeDirectories(includes, target, lang, config);
  
  std::string includeFlags;
  for (const auto& inc : includes) {
    if (!includeFlags.empty()) includeFlags += " ";
    // Convert absolute include paths to relative for Nix build environment
    std::string relativeInclude = cmSystemTools::RelativePath(
      this->GetCMakeInstance()->GetHomeOutputDirectory(), inc);
    includeFlags += "-I" + (!relativeInclude.empty() ? relativeInclude : inc);
  }
  
  nixFileStream << "  " << derivName << " = stdenv.mkDerivation {\n";
  nixFileStream << "    name = \"" << objectName << "\";\n";
  
  // Determine source path - check if this source file is external
  std::string currentSourceDir = cmSystemTools::GetFilenamePath(sourceFile);
  std::string buildDir = this->GetCMakeInstance()->GetHomeOutputDirectory();
  // Since Nix file is now in source directory, use current directory as base
  std::string projectSourceRelPath = ".";
  std::string initialRelativePath = cmSystemTools::RelativePath(this->GetCMakeInstance()->GetHomeDirectory(), sourceFile);
  
  // Check if source file is external (outside project tree)
  bool isExternalSource = (initialRelativePath.find("../") == 0 || cmSystemTools::FileIsFullPath(initialRelativePath));
  
  if (isExternalSource) {
    // For external sources, create a composite source including both project and external file
    nixFileStream << "    src = pkgs.runCommand \"composite-src\" {} ''\n";
    nixFileStream << "      mkdir -p $out\n";
    // Copy project source tree
    if (projectSourceRelPath.empty()) {
      nixFileStream << "      cp -r ${./.}/* $out/ 2>/dev/null || true\n";
    } else {
      nixFileStream << "      cp -r ${./" << projectSourceRelPath << "}/* $out/ 2>/dev/null || true\n";
    }
    // Copy external source file to build dir root
    std::string fileName = cmSystemTools::GetFilenameName(sourceFile);
    nixFileStream << "      cp ${" << sourceFile << "} $out/" << fileName << "\n";
    
    // For ABI detection files, also copy the required header file
    if (fileName.find("CMakeCCompilerABI.c") != std::string::npos ||
        fileName.find("CMakeCXXCompilerABI.cpp") != std::string::npos) {
      std::string abiSourceDir = cmSystemTools::GetFilenamePath(sourceFile);
      nixFileStream << "      cp ${" << abiSourceDir << "/CMakeCompilerABI.h} $out/CMakeCompilerABI.h\n";
    }
    nixFileStream << "    '';\n";
  } else {
    // Regular project source
    if (projectSourceRelPath.empty()) {
      nixFileStream << "    src = ./.;\n";
    } else {
      // Remove trailing slash if present
      if (!projectSourceRelPath.empty() && projectSourceRelPath.back() == '/') {
        projectSourceRelPath.pop_back();
      }
      nixFileStream << "    src = ./" << projectSourceRelPath << ";\n";
    }
  }
  
  // Get external library dependencies for compilation (headers) - use cache
  std::pair<cmGeneratorTarget*, std::string> cacheKey = {target, config};
  std::vector<std::string> libraryDeps;
  auto libCacheIt = this->LibraryDependencyCache.find(cacheKey);
  if (libCacheIt != this->LibraryDependencyCache.end()) {
    libraryDeps = libCacheIt->second;
  } else {
    // Fallback: compute and cache
    auto targetGen = cmNixTargetGenerator::New(target);
    libraryDeps = targetGen->GetTargetLibraryDependencies(config);
    this->LibraryDependencyCache[cacheKey] = libraryDeps;
  }
  
  // Build buildInputs list including external libraries for headers
  std::string compilerPkg = this->GetCompilerPackage(lang);
  nixFileStream << "    buildInputs = [ " << compilerPkg;
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
          nixFileStream << " " << actualPkg;
        }
      } else {
        // This is a file import - adjust path for subdirectory sources
        if (!projectSourceRelPath.empty() && lib.find("./") == 0) {
          // Convert relative path to be relative to project root
          nixFileStream << " (import " << projectSourceRelPath << "/" << lib.substr(2) << " { inherit pkgs; })";
        } else {
          nixFileStream << " (import " << lib << " { inherit pkgs; })";
        }
      }
    }
  }
  
  // Check if this source file is generated by a custom command
  std::string customCommandDep;
  auto it = this->CustomCommandOutputs.find(sourceFile);
  if (it != this->CustomCommandOutputs.end()) {
    customCommandDep = it->second;
    nixFileStream << " " << customCommandDep;
  }
  
  nixFileStream << " ];\n";
  
  nixFileStream << "    dontFixup = true;\n";
  
  if (!headers.empty()) {
    nixFileStream << "    # Header dependencies\n";
    nixFileStream << "    propagatedInputs = [\n";
    for (const std::string& header : headers) {
      if (isExternalSource) {
        // For external sources, headers are copied into the composite source root
        nixFileStream << "      ./" << header << "\n";
      } else {
        // For project sources, headers are relative to the source directory
        if (projectSourceRelPath.empty()) {
          // Source is current directory, headers are relative to it
          nixFileStream << "      ./" << header << "\n";
        } else {
          // Source is a subdirectory (e.g., ext/opencv), headers are relative to that source directory
          // Use the same source path as the derivation
          nixFileStream << "      " << projectSourceRelPath << "/" << header << "\n";
        }
      }
    }
    nixFileStream << "    ];\n";
  }
  
  nixFileStream << "    # Configuration: " << config << "\n";
  nixFileStream << "    buildPhase = ''\n";
  
  // Determine the source path - always use source directory as base
  std::string sourcePath;
  if (!customCommandDep.empty()) {
    // Source is generated by a custom command - reference from derivation output
    std::string fileName = cmSystemTools::GetFilenameName(sourceFile);
    sourcePath = "${" + customCommandDep + "}/" + fileName;
  } else {
    // All files (source and generated) - use relative path from source directory
    std::string projectSourceDir = this->GetCMakeInstance()->GetHomeDirectory();
    std::string sourceFileRelativePath = cmSystemTools::RelativePath(projectSourceDir, sourceFile);
    
    // Check if this is an external file (outside project tree)
    if (sourceFileRelativePath.find("../") == 0 || cmSystemTools::FileIsFullPath(sourceFileRelativePath)) {
      // External file - use just the filename, it will be copied to source dir
      std::string fileName = cmSystemTools::GetFilenameName(sourceFile);
      sourcePath = fileName;
    }
    else {
      // File within project tree (source or generated)
      sourcePath = sourceFileRelativePath;
    }
  }
  
  // Combine all flags: compile flags + defines + includes
  std::string allFlags;
  if (!compileFlags.empty()) allFlags += compileFlags + " ";
  if (!defineFlags.empty()) allFlags += defineFlags + " ";
  if (!includeFlags.empty()) allFlags += includeFlags + " ";
  
  // Add -fPIC for shared and module libraries
  if (target->GetType() == cmStateEnums::SHARED_LIBRARY ||
      target->GetType() == cmStateEnums::MODULE_LIBRARY) {
    allFlags += "-fPIC ";
  }
  
  // Remove trailing space
  if (!allFlags.empty() && allFlags.back() == ' ') {
    allFlags.pop_back();
  }
  
  std::string compilerCmd = this->GetCompilerCommand(lang);
  nixFileStream << "      " << compilerCmd << " -c " << allFlags << " \"" << sourcePath 
    << "\" -o \"$out\"\n";
  nixFileStream << "    '';\n";
  nixFileStream << "    installPhase = \"true\"; # No install needed for objects\n";
  nixFileStream << "  };\n\n";
}



void cmGlobalNixGenerator::WriteLinkDerivation(
  cmGeneratedFileStream& nixFileStream, cmGeneratorTarget* target)
{
  std::string derivName = this->GetDerivationName(target->GetName());
  std::string targetName = target->GetName();
  
  // Determine source path for subdirectory adjustment
  std::string sourceDir = this->GetCMakeInstance()->GetHomeDirectory();
  std::string buildDir = this->GetCMakeInstance()->GetHomeOutputDirectory();
  std::string projectSourceRelPath = cmSystemTools::RelativePath(buildDir, sourceDir);
  
  // Check if this is a try_compile
  bool isTryCompile = buildDir.find("CMakeScratch") != std::string::npos;
  
  std::cerr << "[NIX-TRACE] " << __FILE__ << ":" << __LINE__ 
            << " WriteLinkDerivation for target: " << targetName
            << " buildDir: " << buildDir
            << " isTryCompile: " << (isTryCompile ? "true" : "false") << std::endl;
  
  nixFileStream << "  " << derivName << " = stdenv.mkDerivation {\n";
  
  // Generate appropriate name for target type
  std::string outputName;
  if (target->GetType() == cmStateEnums::SHARED_LIBRARY) {
    outputName = this->GetLibraryPrefix() + targetName + this->GetSharedLibraryExtension();
  } else if (target->GetType() == cmStateEnums::MODULE_LIBRARY) {
    outputName = targetName + this->GetSharedLibraryExtension();  // Modules typically don't have lib prefix
  } else {
    outputName = targetName;
  }
  
  nixFileStream << "    name = \"" << outputName << "\";\n";
  
  // Get external library dependencies using cache
  std::string config = this->GetBuildConfiguration(target);
  
  std::pair<cmGeneratorTarget*, std::string> cacheKey = {target, config};
  std::vector<std::string> libraryDeps;
  auto libCacheIt = this->LibraryDependencyCache.find(cacheKey);
  if (libCacheIt != this->LibraryDependencyCache.end()) {
    libraryDeps = libCacheIt->second;
  } else {
    // Fallback: compute and cache
    auto targetGen = cmNixTargetGenerator::New(target);
    libraryDeps = targetGen->GetTargetLibraryDependencies(config);
    this->LibraryDependencyCache[cacheKey] = libraryDeps;
  }
  
  // Get link implementation for dependency processing
  auto linkImpl = target->GetLinkImplementation(config, cmGeneratorTarget::UseTo::Compile);
  
  // Build buildInputs list including external libraries
  // Determine the primary language for linking
  std::string primaryLang = "C";
  std::vector<cmSourceFile*> sources;
  target->GetSourceFiles(sources, "");
  for (cmSourceFile* source : sources) {
    if (source->GetLanguage() == "CXX") {
      primaryLang = "CXX";
      break;  // C++ takes precedence
    }
  }
  
  std::string compilerPkg = this->GetCompilerPackage(primaryLang);
  nixFileStream << "    buildInputs = [ " << compilerPkg;
  
  // Add external library dependencies
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
          nixFileStream << " " << actualPkg;
        }
      } else {
        // This is a file import - adjust path for subdirectory sources
        if (!projectSourceRelPath.empty() && lib.find("./") == 0) {
          // Convert relative path to be relative to project root
          nixFileStream << " (import " << projectSourceRelPath << "/" << lib.substr(2) << " { inherit pkgs; })";
        } else {
          nixFileStream << " (import " << lib << " { inherit pkgs; })";
        }
      }
    }
  }
  
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
          nixFileStream << " " << depDerivName;
          directSharedDeps.insert(depTargetName); // Track direct deps to avoid duplication
        }
      }
    }
  }
  
  // Add transitive shared library dependencies to buildInputs (excluding direct ones)
  for (const std::string& depTarget : transitiveDeps) {
    if (directSharedDeps.find(depTarget) == directSharedDeps.end()) {
      std::string depDerivName = this->GetDerivationName(depTarget);
      nixFileStream << " " << depDerivName;
    }
  }
  
  nixFileStream << " ];\n";
  
  nixFileStream << "    dontUnpack = true;\n";  // No source to unpack
  
  // Collect object file dependencies (reuse sources from above)
  
  nixFileStream << "    objects = [\n";
  for (cmSourceFile* source : sources) {
    std::string const& lang = source->GetLanguage();
    if (lang == "C" || lang == "CXX") {
      std::string objDerivName = this->GetDerivationName(
        target->GetName(), source->GetFullPath());
      nixFileStream << "      " << objDerivName << "\n";
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
              nixFileStream << "      " << objDerivName << "\n";
              goto next_external_object;
            }
          }
        }
      }
    }
    next_external_object:;
  }
  
  nixFileStream << "    ];\n";
  
  // Target dependencies will be referenced directly in link flags
  
  // Get library link flags for build phase
  std::string linkFlags;
  if (linkImpl) {
    for (const cmLinkItem& item : linkImpl->Libraries) {
      if (item.Target && item.Target->IsImported()) {
        // This is an imported target from find_package
        std::string importedTargetName = item.Target->GetName();
        // Need to create target generator for package mapper access
        auto targetGen = cmNixTargetGenerator::New(target);
        std::string flags = targetGen->GetPackageMapper().GetLinkFlags(importedTargetName);
        if (!flags.empty()) {
          linkFlags += " " + flags;
        }
      } else if (item.Target && !item.Target->IsImported()) {
        // This is a CMake target within the same project
        std::string depTargetName = item.Target->GetName();
        std::string depDerivName = this->GetDerivationName(depTargetName);
        
        // Add appropriate link flags based on target type using direct references
        if (item.Target->GetType() == cmStateEnums::SHARED_LIBRARY) {
          // For shared libraries, use Nix string interpolation
          linkFlags += " ${" + depDerivName + "}/" + this->GetLibraryPrefix() + depTargetName + this->GetSharedLibraryExtension();
        } else if (item.Target->GetType() == cmStateEnums::MODULE_LIBRARY) {
          // For module libraries, use Nix string interpolation (no lib prefix)
          linkFlags += " ${" + depDerivName + "}/" + depTargetName + this->GetSharedLibraryExtension();
        } else if (item.Target->GetType() == cmStateEnums::STATIC_LIBRARY) {
          // For static libraries, link the archive directly using string interpolation
          linkFlags += " ${" + depDerivName + "}";
        }
      } else if (!item.Target) { 
        // External library (not a target)
        std::string libName = item.AsStr();
        linkFlags += " -l" + libName;
      }
    }
  }
  
  // Add transitive shared library dependencies to linkFlags (excluding direct ones)
  for (const std::string& depTarget : transitiveDeps) {
    if (directSharedDeps.find(depTarget) == directSharedDeps.end()) {
      std::string depDerivName = this->GetDerivationName(depTarget);
      linkFlags += " ${" + depDerivName + "}/" + this->GetLibraryPrefix() + depTarget + this->GetSharedLibraryExtension();
    }
  }
  
  std::string linkCompilerCmd = this->GetCompilerCommand(primaryLang);
  nixFileStream << "    buildPhase = ''\n";
  if (target->GetType() == cmStateEnums::EXECUTABLE) {
    nixFileStream << "      " << linkCompilerCmd << " $objects" << linkFlags << " -o \"$out\"\n";
  } else if (target->GetType() == cmStateEnums::STATIC_LIBRARY) {
    nixFileStream << "      ar rcs \"$out\" $objects\n";
  } else if (target->GetType() == cmStateEnums::SHARED_LIBRARY) {
    // Get library version properties
    cmValue version = target->GetProperty("VERSION");
    cmValue soversion = target->GetProperty("SOVERSION");
    
    nixFileStream << "      mkdir -p $out\n";
    std::string libName = this->GetLibraryPrefix() + targetName + this->GetSharedLibraryExtension();
    
    if (version && soversion) {
      // Create versioned library and symlinks
      std::string versionedName = libName + "." + *version;
      std::string soversionName = libName + "." + *soversion;
      
      nixFileStream << "      " << linkCompilerCmd << " -shared $objects" << linkFlags 
                    << " -Wl,-soname," << soversionName << " -Wl,-rpath,$out/lib -o $out/" << versionedName << "\n";
      nixFileStream << "      ln -sf " << versionedName << " $out/" << soversionName << "\n";
      nixFileStream << "      ln -sf " << versionedName << " $out/" << libName << "\n";
    } else {
      // Simple shared library without versioning
      nixFileStream << "      " << linkCompilerCmd << " -shared $objects" << linkFlags 
                    << " -Wl,-rpath,$out/lib -o $out/" << libName << "\n";
    }
  } else if (target->GetType() == cmStateEnums::MODULE_LIBRARY) {
    // Module libraries are like shared libraries but without versioning or lib prefix
    nixFileStream << "      mkdir -p $out\n";
    std::string modName = targetName + this->GetSharedLibraryExtension();
    nixFileStream << "      " << linkCompilerCmd << " -shared $objects" << linkFlags 
                  << " -o $out/" << modName << "\n";
  }
  nixFileStream << "    '';\n";
  
  // For try_compile, copy the output file where CMake expects it for COPY_FILE
  if (isTryCompile) {
    std::cerr << "[NIX-TRACE] " << __FILE__ << ":" << __LINE__ 
              << " Adding try_compile output file handling for: " << targetName << std::endl;
    
    nixFileStream << "    # Handle try_compile COPY_FILE requirement\n";
    nixFileStream << "    postBuildPhase = ''\n";
    nixFileStream << "      # Create output location in build directory for CMake COPY_FILE\n";
    nixFileStream << "      COPY_DEST=\"" << buildDir << "/" << targetName << "\"\n";
    nixFileStream << "      cp \"$out\" \"$COPY_DEST\"\n";
    nixFileStream << "      echo '[NIX-TRACE] Copied try_compile output to: '$COPY_DEST\n";
    nixFileStream << "      # Write location file that CMake expects to find the executable path\n";
    nixFileStream << "      echo \"$COPY_DEST\" > \"" << buildDir << "/" << targetName << "_loc\"\n";
    nixFileStream << "      echo '[NIX-TRACE] Wrote location file: " << buildDir << "/" << targetName << "_loc'\n";
    nixFileStream << "      echo '[NIX-TRACE] Location file contains: '$COPY_DEST\n";
    nixFileStream << "    '';\n";
  }
  
  nixFileStream << "    installPhase = \"true\"; # No install needed\n";
  nixFileStream << "  };\n\n";
}

std::vector<std::string> cmGlobalNixGenerator::GetSourceDependencies(
  std::string const& /*sourceFile*/) const
{
  // TODO: Implement header dependency tracking
  // This will use CMake's existing dependency analysis
  return {};
}

std::string cmGlobalNixGenerator::GetCompilerPackage(const std::string& lang) const
{
  // Check cache first
  auto it = this->CompilerPackageCache.find(lang);
  if (it != this->CompilerPackageCache.end()) {
    return it->second;
  }
  
  cmake* cm = this->GetCMakeInstance();
  std::string compilerIdVar = "CMAKE_" + lang + "_COMPILER_ID";
  std::string compilerVar = "CMAKE_" + lang + "_COMPILER";
  
  cmValue compilerId = cm->GetState()->GetGlobalProperty(compilerIdVar);
  if (!compilerId) {
    compilerId = cm->GetCacheDefinition(compilerIdVar);
  }

  std::string result;
  if (lang == "CUDA") {
    // CUDA requires special package
    result = "cudatoolkit";
  } else if (lang == "Swift") {
    // Swift requires special package
    result = "swift";
  } else if (lang == "ASM_NASM") {
    // NASM requires special package
    result = "nasm";
  } else if (compilerId) {
    std::string id = *compilerId;
    if (id == "GNU") {
      result = "gcc";
    } else if (id == "Clang" || id == "AppleClang") {
      result = "clang";
    } else if (id == "Intel") {
      result = "intel-compiler";
    } else if (id == "PGI") {
      result = "pgi";
    } else if (id == "MSVC") {
      // For future Windows support
      result = "msvc";
    } else {
      // Fallback to executable name
      cmValue compiler = cm->GetCacheDefinition(compilerVar);
      if(compiler) {
        std::string compilerName = cmSystemTools::GetFilenameName(*compiler);
        if (compilerName.find("clang") != std::string::npos) {
          result = "clang";
        } else if (compilerName.find("gcc") != std::string::npos) {
          result = "gcc";
        } else {
          result = "gcc"; // Default fallback
        }
      } else {
        result = "gcc"; // Default fallback
      }
    }
  } else {
    result = "gcc"; // Default fallback
  }

  if (cm->GetState()->GetGlobalPropertyAsBool("CMAKE_CROSSCOMPILING")) {
    result += "-cross";
  }
  
  // Cache the result
  this->CompilerPackageCache[lang] = result;
  return result;
}

std::string cmGlobalNixGenerator::GetCompilerCommand(const std::string& lang) const
{
  // Check cache first
  auto it = this->CompilerCommandCache.find(lang);
  if (it != this->CompilerCommandCache.end()) {
    return it->second;
  }
  
  // In Nix, we use the compiler from the Nix package
  // The actual command depends on the package and language
  std::string compilerPkg = this->GetCompilerPackage(lang);
  
  std::string result;
  if (lang == "Fortran") {
    if (compilerPkg == "gcc") {
      result = "gfortran";
    } else if (compilerPkg == "intel-compiler") {
      result = "ifort";
    } else {
      result = "gfortran"; // Default Fortran compiler
    }
  } else if (lang == "CUDA") {
    result = "nvcc"; // NVIDIA CUDA compiler
  } else if (lang == "Swift") {
    result = "swiftc"; // Swift compiler
  } else if (lang == "ASM" || lang == "ASM-ATT") {
    // Assembly language - use the same compiler as C
    result = (compilerPkg == "clang") ? "clang" : "gcc";
  } else if (lang == "ASM_NASM") {
    result = "nasm"; // NASM assembler
  } else if (lang == "ASM_MASM") {
    result = "ml"; // MASM assembler (for Windows compatibility)
  } else if (compilerPkg == "gcc") {
    result = (lang == "CXX") ? "g++" : "gcc";
  } else if (compilerPkg == "clang") {
    result = (lang == "CXX") ? "clang++" : "clang";
  } else {
    // Default fallback
    result = (lang == "CXX") ? "g++" : "gcc";
  }
  
  // Cache the result
  this->CompilerCommandCache[lang] = result;
  return result;
}

std::string cmGlobalNixGenerator::GetBuildConfiguration(cmGeneratorTarget* target) const
{
  std::string config = target->Target->GetMakefile()->GetSafeDefinition("CMAKE_BUILD_TYPE");
  if (config.empty()) {
    config = "Release"; // Default to Release if no configuration specified
  }
  return config;
}



void cmGlobalNixGenerator::WriteInstallOutputs(cmGeneratedFileStream& nixFileStream)
{
  for (cmGeneratorTarget* target : this->InstallTargets) {
    std::string targetName = target->GetName();
    std::string derivName = this->GetDerivationName(targetName);
    std::string installDerivName = derivName + "_install";
    
    nixFileStream << "  \"" << targetName << "_install\" = " << installDerivName << ";\n";
  }
}

void cmGlobalNixGenerator::CollectInstallTargets()
{
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
    
    nixFileStream << "      mkdir -p $out/" << dest << "\n";
    
    // Determine installation destination based on target type
    if (target->GetType() == cmStateEnums::EXECUTABLE) {
      nixFileStream << "      cp $src $out/" << dest << "/" << targetName << "\n";
    } else if (target->GetType() == cmStateEnums::SHARED_LIBRARY) {
      nixFileStream << "      cp -r $src/* $out/" << dest << "/ 2>/dev/null || true\n";
    } else if (target->GetType() == cmStateEnums::STATIC_LIBRARY) {
      nixFileStream << "      cp $src $out/" << dest << "/" << this->GetLibraryPrefix() << targetName << this->GetStaticLibraryExtension() << "\n";
    }
    
    nixFileStream << "    '';\n";
    nixFileStream << "  };\n\n";
  }
}

// Dependency graph implementation
void cmGlobalNixGenerator::BuildDependencyGraph() {
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