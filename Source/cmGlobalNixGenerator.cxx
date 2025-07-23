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
#include <cctype>

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
#include "cmOutputConverter.h"
#include "cmNixWriter.h"
#include "cmStringAlgorithms.h"

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
  if (this->GetCMakeInstance()->GetDebugOutput()) {
    std::cerr << "[NIX-TRACE] " << __FILE__ << ":" << __LINE__ << " Generate() started" << std::endl;
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
  
  // First call the parent Generate to set up targets
  this->cmGlobalGenerator::Generate();
  
  if (this->GetCMakeInstance()->GetDebugOutput()) {
    std::cerr << "[NIX-TRACE] " << __FILE__ << ":" << __LINE__ << " Parent Generate() completed" << std::endl;
  }
  
  // Build dependency graph for transitive dependency resolution
  this->BuildDependencyGraph();
  
  // Generate our Nix output
  this->WriteNixFile();
  
  if (this->GetCMakeInstance()->GetDebugOutput()) {
    std::cerr << "[NIX-TRACE] " << __FILE__ << ":" << __LINE__ << " Generate() completed" << std::endl;
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
    std::cerr << "[NIX-TRACE] " << __FILE__ << ":" << __LINE__ 
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
  
  // For try_compile, look for default.nix in the scratch directory without suffix
  if (isTryCompile) {
    // Extract the base scratch directory (remove numeric suffix if present)
    std::string scratchDir = projectDir;
    size_t underscorePos = scratchDir.find_last_of('_');
    if (underscorePos != std::string::npos) {
      // Check if everything after the underscore is numeric
      std::string suffix = scratchDir.substr(underscorePos + 1);
      bool isNumeric = !suffix.empty() && std::all_of(suffix.begin(), suffix.end(), 
        [](unsigned char c) { return std::isdigit(c); });
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
    if (this->GetCMakeInstance()->GetDebugOutput()) {
      std::cerr << "[NIX-TRACE] " << __FILE__ << ":" << __LINE__ 
                << " Generating try-compile copy commands" << std::endl;
    }
    
    GeneratedMakeCommand copyCommand;
    copyCommand.Add("sh");
    copyCommand.Add("-c");
    
    std::string copyScript = "set -e; ";
    for (auto const& tname : targetNames) {
      if (!tname.empty()) {
        if (this->GetCMakeInstance()->GetDebugOutput()) {
          std::cerr << "[NIX-TRACE] " << __FILE__ << ":" << __LINE__ 
                    << " Adding copy command for target: " << tname << std::endl;
        }
        
        // Read the target location file and copy the binary
        std::string escapedTargetName = cmOutputConverter::EscapeForShell(tname, cmOutputConverter::Shell_Flag_IsUnix);
        std::string locationFile = escapedTargetName + "_loc";
        copyScript += "if [ -f " + locationFile + " ]; then ";
        copyScript += "TARGET_LOCATION=$(cat " + locationFile + "); ";
        if (this->GetCMakeInstance()->GetDebugOutput()) {
          copyScript += "echo '[NIX-TRACE] Target location: '$TARGET_LOCATION; ";
        }
        copyScript += "if [ -f \"result\" ]; then ";
        copyScript += "STORE_PATH=$(readlink result); ";
        if (this->GetCMakeInstance()->GetDebugOutput()) {
          copyScript += "echo '[NIX-TRACE] Store path: '$STORE_PATH; ";
        }
        copyScript += "cp \"$STORE_PATH\" \"$TARGET_LOCATION\" 2>/dev/null";
        if (this->GetCMakeInstance()->GetDebugOutput()) {
          copyScript += " || echo '[NIX-TRACE] Copy failed'";
        }
        copyScript += "; ";
        if (this->GetCMakeInstance()->GetDebugOutput()) {
          copyScript += "else echo '[NIX-TRACE] No result symlink found'; ";
        }
        copyScript += "fi; ";
        if (this->GetCMakeInstance()->GetDebugOutput()) {
          copyScript += "else echo '[NIX-TRACE] No location file for " + cmOutputConverter::EscapeForShell(escapedTargetName, cmOutputConverter::Shell_Flag_IsUnix) + "'; ";
        }
        copyScript += "fi; ";
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
  
  if (this->GetCMakeInstance()->GetDebugOutput()) {
    std::cerr << "[NIX-TRACE] WriteNixFile() writing to: " << nixFile << std::endl;
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
    std::cerr << "[NIX-TRACE] Opened Nix file successfully, starting to write..." << std::endl;
  }

  // Use NixWriter for cleaner code generation
  cmNixWriter writer(nixFileStream);
  
  // Write Nix file header
  writer.WriteComment("Generated by CMake Nix Generator");
  writer.WriteLine("with import <nixpkgs> {};");
  writer.WriteLine("with pkgs;");
  writer.WriteLine();
  writer.StartLetBinding();

  // Collect all custom commands with proper thread safety
  // Use a local copy to avoid race conditions
  std::vector<CustomCommandInfo> localCustomCommands;
  std::map<std::string, std::string> localCustomCommandOutputs;
  
  // First pass: Collect all custom commands
  {
    std::lock_guard<std::mutex> lock(this->CustomCommandMutex);
    this->CustomCommands.clear();
    this->CustomCommandOutputs.clear();
  }
  
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
            
            {
              std::lock_guard<std::mutex> lock(this->CustomCommandMutex);
              this->CustomCommands.push_back(info);
              
              // Populate CustomCommandOutputs map for dependency tracking
              for (const std::string& output : info.Outputs) {
                this->CustomCommandOutputs[output] = info.DerivationName;
              }
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
  
  // Create thread-safe local copies for dependency processing
  {
    std::lock_guard<std::mutex> lock(this->CustomCommandMutex);
    localCustomCommands = this->CustomCommands;
    localCustomCommandOutputs = this->CustomCommandOutputs;
  }
  
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
  
  // Build dependency graph using indices
  for (size_t i = 0; i < localCustomCommands.size(); ++i) {
    const auto& info = localCustomCommands[i];
    for (const std::string& dep : info.Depends) {
      auto depIt = localCustomCommandOutputs.find(dep);
      if (depIt != localCustomCommandOutputs.end()) {
        // Find the index of the dependency
        for (size_t j = 0; j < localCustomCommands.size(); ++j) {
          if (localCustomCommands[j].DerivationName == depIt->second) {
            dependents[depIt->second].push_back(i);
            inDegree[info.DerivationName]++;
            break;
          }
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
      std::cerr << "[DEBUG] Total custom commands: " << localCustomCommands.size() << std::endl;
      std::cerr << "[DEBUG] Ordered commands: " << orderedCommands.size() << std::endl;
      for (size_t i = 0; i < localCustomCommands.size(); ++i) {
        std::cerr << "[DEBUG] Command " << i << ": " << localCustomCommands[i].DerivationName << std::endl;
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
      std::cerr << "[DEBUG] Unprocessed commands: " << cyclicCommands.size() << std::endl;
      for (size_t idx : cyclicCommands) {
        const auto& cmd = localCustomCommands[idx];
        std::cerr << "[DEBUG] Unprocessed: " << cmd.DerivationName << " (indegree=" << inDegree[cmd.DerivationName] << ")" << std::endl;
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
    const int MAX_CYCLE_DEPTH = 100;
    std::function<bool(const std::string&, std::set<std::string>&, std::vector<std::string>&, int)> findCycle;
    findCycle = [&](const std::string& current, std::set<std::string>& visited, std::vector<std::string>& path, int depth) -> bool {
      // Prevent stack overflow with depth limit
      if (depth > MAX_CYCLE_DEPTH) {
        std::cerr << "[WARNING] Cycle detection depth limit exceeded at: " << current << std::endl;
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
      std::cerr << "WARNING: Circular dependencies detected, but proceeding due to CMAKE_NIX_IGNORE_CIRCULAR_DEPS=ON" << std::endl;
      std::cerr << "WARNING: This may result in incorrect build order and build failures." << std::endl;
      std::cerr << "WARNING: " << (this->CustomCommands.size() - orderedCommands.size()) << " commands have circular dependencies but will be processed anyway." << std::endl;
      
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
      
      std::cerr << "INFO: Now processing all " << orderedCommands.size() << " custom commands." << std::endl;
      std::cerr << "DEBUG: About to write custom commands to Nix file..." << std::endl;
    } else {
      this->GetCMakeInstance()->IssueMessage(MessageType::FATAL_ERROR, msg.str());
      return;
    }
  }
  
  // Write commands in order
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
      cmNixCustomCommandGenerator ccg(info->Command, info->LocalGen, config);
      ccg.Generate(nixFileStream);
    } catch (const std::exception& e) {
      std::cerr << "Exception writing custom command " << info->DerivationName << ": " << e.what() << std::endl;
    } catch (...) {
      std::cerr << "Unknown exception writing custom command " << info->DerivationName << std::endl;
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
        
        for (cmSourceFile* source : sources) {
          // Skip Unity-generated source files as we don't support Unity builds
          if (source->GetProperty("UNITY_SOURCE_FILE")) {
            continue;
          }
          
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
  
  // Cache the result
  {
    std::lock_guard<std::mutex> lock(this->CacheMutex);
    this->DerivationNameCache[cacheKey] = result;
  }
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
  cmNixWriter writer(nixFileStream);
  
  std::string sourceFile = source->GetFullPath();
  
  // Validate source path
  if (sourceFile.empty()) {
    std::ostringstream msg;
    msg << "Empty source file path for target " << target->GetName();
    this->GetCMakeInstance()->IssueMessage(MessageType::WARNING, msg.str());
    return;
  }
  
  // Check if file exists (unless it's a generated file)
  if (!source->GetIsGenerated() && !cmSystemTools::FileExists(sourceFile)) {
    std::ostringstream msg;
    msg << "Source file does not exist: " << sourceFile << " for target " << target->GetName();
    this->GetCMakeInstance()->IssueMessage(MessageType::WARNING, msg.str());
    // Continue anyway as it might be generated later
  }
  
  // Validate path doesn't contain dangerous characters that could break Nix expressions
  if (sourceFile.find('"') != std::string::npos || 
      sourceFile.find('$') != std::string::npos ||
      sourceFile.find('`') != std::string::npos) {
    std::ostringstream msg;
    msg << "Source file path contains potentially dangerous characters: " << sourceFile;
    this->GetCMakeInstance()->IssueMessage(MessageType::WARNING, msg.str());
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
  
  // Add PCH compile options if applicable
  // First check if this is a PCH source file
  std::vector<std::string> pchArchs = target->GetPchArchs(config, lang);
  std::unordered_set<std::string> pchSources;
  for (const std::string& arch : pchArchs) {
    std::string pchSource = target->GetPchSource(config, lang, arch);
    if (!pchSource.empty()) {
      pchSources.insert(pchSource);
    }
  }
  
  // Check if source file has SKIP_PRECOMPILE_HEADERS property
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
      std::string projectDir = this->GetCMakeInstance()->GetHomeDirectory();
      size_t pos = 0;
      while ((pos = processedOptions.find(projectDir, pos)) != std::string::npos) {
        // Find the end of the path (space or end of string)
        size_t endPos = processedOptions.find(' ', pos);
        if (endPos == std::string::npos) {
          endPos = processedOptions.length();
        }
        
        // Extract the full path
        std::string fullPath = processedOptions.substr(pos, endPos - pos);
        
        // Convert to relative path
        std::string relPath = cmSystemTools::RelativePath(projectDir, fullPath);
        
        // Replace in the string
        processedOptions.replace(pos, fullPath.length(), relPath);
        
        // Move past this replacement
        pos += relPath.length();
      }
      
      if (!compileFlags.empty()) compileFlags += " ";
      compileFlags += processedOptions;
    }
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
    // Skip include directories from Nix store - these are provided by buildInputs packages
    if (inc.find("/nix/store/") != std::string::npos) {
      continue;
    }
    
    if (!includeFlags.empty()) includeFlags += " ";
    // Convert absolute include paths to relative for Nix build environment
    std::string relativeInclude = cmSystemTools::RelativePath(
      this->GetCMakeInstance()->GetHomeOutputDirectory(), inc);
    includeFlags += "-I" + (!relativeInclude.empty() ? relativeInclude : inc);
  }
  
  // Start the derivation
  writer.StartDerivation(derivName, 1);
  writer.WriteAttribute("name", objectName);
  
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
    writer.WriteIndented(2, "src = pkgs.runCommand \"composite-src\" {} ''");
    writer.WriteIndented(3, "mkdir -p $out");
    // Copy project source tree
    if (projectSourceRelPath.empty()) {
      writer.WriteIndented(3, "cp -r ${./.}/* $out/ 2>/dev/null || true");
    } else {
      writer.WriteIndented(3, "cp -r ${./" + projectSourceRelPath + "}/* $out/ 2>/dev/null || true");
    }
    // Copy external source file to build dir root
    std::string fileName = cmSystemTools::GetFilenameName(sourceFile);
    writer.WriteIndented(3, "cp ${" + sourceFile + "} $out/" + fileName);
    
    // For ABI detection files, also copy the required header file
    if (fileName.find("CMakeCCompilerABI.c") != std::string::npos ||
        fileName.find("CMakeCXXCompilerABI.cpp") != std::string::npos) {
      std::string abiSourceDir = cmSystemTools::GetFilenamePath(sourceFile);
      writer.WriteIndented(3, "cp ${" + abiSourceDir + "/CMakeCompilerABI.h} $out/CMakeCompilerABI.h");
    }
    writer.WriteIndented(2, "'';");
  } else {
    // Regular project source - always use fileset for better performance
    auto targetGen = cmNixTargetGenerator::New(target);
    std::vector<std::string> dependencies = targetGen->GetSourceDependencies(source);
    
    // Create a list of files for the fileset
    std::vector<std::string> fileList;
    
    // Add the main source file - always add it
    std::string relativeSource = cmSystemTools::RelativePath(
      this->GetCMakeInstance()->GetHomeDirectory(), sourceFile);
    if (!relativeSource.empty() && relativeSource.find("../") != 0) {
      fileList.push_back(relativeSource);
    }
    
    // Add dependencies (headers)
    // Note: headers from ObjectDerivation are already computed and stored
    for (const auto& dep : headers) {
      // Convert absolute path to relative if needed
      std::string relDep;
      if (cmSystemTools::FileIsFullPath(dep)) {
        relDep = cmSystemTools::RelativePath(
          this->GetCMakeInstance()->GetHomeDirectory(), dep);
      } else {
        relDep = dep;
      }
      
      // Only add if it's within the project and exists
      if (!relDep.empty() && relDep.find("../") != 0) {
        // Verify file exists (unless it's generated)
        std::string fullPath = dep;
        if (!cmSystemTools::FileIsFullPath(dep)) {
          fullPath = this->GetCMakeInstance()->GetHomeDirectory();
          fullPath += "/";
          fullPath += dep;
        }
        
        if (cmSystemTools::FileExists(fullPath) || source->GetIsGenerated()) {
          fileList.push_back(relDep);
        }
      }
    }
    
    // Use fileset union for minimal source sets
    if (!fileList.empty()) {
      writer.WriteFilesetUnionSrcAttribute(fileList);
    } else {
      // Fallback if no files detected
      writer.WriteSourceAttribute("./.");
    }
  }
  
  // Get external library dependencies for compilation (headers)
  std::vector<std::string> libraryDeps = this->GetCachedLibraryDependencies(target, config);
  
  // Build buildInputs list including external libraries for headers
  std::vector<std::string> buildInputs;
  std::string compilerPkg = this->GetCompilerPackage(lang);
  buildInputs.push_back(compilerPkg);
  
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
          // Convert relative path to be relative to project root
          buildInputs.push_back("(import " + projectSourceRelPath + "/" + 
                               lib.substr(2) + " { inherit pkgs; })");
        } else {
          buildInputs.push_back("(import " + lib + " { inherit pkgs; })");
        }
      }
    }
  }
  
  // Check if this source file is generated by a custom command
  auto it = this->CustomCommandOutputs.find(sourceFile);
  if (it != this->CustomCommandOutputs.end()) {
    buildInputs.push_back(it->second);
  }
  
  writer.WriteListAttribute("buildInputs", buildInputs);
  
  writer.WriteAttributeBool("dontFixup", true);
  
  // Filter out external headers (from /nix/store) and collect only project headers
  std::vector<std::string> projectHeaders;
  for (const std::string& header : headers) {
    // Skip headers from Nix store - they're provided by buildInputs packages
    if (header.find("/nix/store/") != std::string::npos) {
      continue;
    }
    // Skip absolute paths outside project (system headers)
    if (cmSystemTools::FileIsFullPath(header)) {
      continue;
    }
    projectHeaders.push_back(header);
  }
  
  if (!projectHeaders.empty()) {
    writer.WriteComment("Header dependencies");
    std::vector<std::string> propagatedInputsList;
    for (const std::string& header : projectHeaders) {
      if (isExternalSource) {
        // For external sources, headers are copied into the composite source root
        propagatedInputsList.push_back("./" + header);
      } else {
        // For project sources, headers are relative to the source directory
        if (projectSourceRelPath.empty()) {
          // Source is current directory, headers are relative to it
          propagatedInputsList.push_back("./" + header);
        } else {
          // Source is a subdirectory (e.g., ext/opencv), headers are relative to that source directory
          // Use the same source path as the derivation
          propagatedInputsList.push_back(projectSourceRelPath + "/" + header);
        }
      }
    }
    writer.WriteListAttribute("propagatedInputs", propagatedInputsList);
  }
  
  writer.WriteComment("Configuration: " + config);
  writer.StartMultilineString("buildPhase");
  
  // Check if this source file is generated by a custom command
  std::string customCommandDep;
  auto customIt = this->CustomCommandOutputs.find(sourceFile);
  if (customIt != this->CustomCommandOutputs.end()) {
    customCommandDep = customIt->second;
  }
  
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
    } else {
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
  std::string buildCommand = compilerCmd + " -c " + allFlags + " \"" + sourcePath 
    + "\" -o \"$out\"";
  writer.WriteMultilineLine(buildCommand);
  writer.EndMultilineString();
  
  writer.WriteAttribute("installPhase", "true"); // No install needed for objects
  writer.EndDerivation(1);
  writer.WriteLine();
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
  
  if (this->GetCMakeInstance()->GetDebugOutput()) {
    std::cerr << "[NIX-TRACE] " << __FILE__ << ":" << __LINE__ 
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
  
  // Start derivation
  writer.StartDerivation(derivName, 1);
  writer.WriteAttribute("name", outputName);
  
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
  
  for (cmSourceFile* source : sources) {
    // Skip Unity-generated source files as we don't support Unity builds
    if (source->GetProperty("UNITY_SOURCE_FILE")) {
      continue;
    }
    
    std::string const& lang = source->GetLanguage();
    if (lang == "C" || lang == "CXX" || lang == "Fortran" || lang == "CUDA") {
      // Exclude PCH source files from linking
      if (pchSources.find(source->GetFullPath()) == pchSources.end()) {
        std::string objDerivName = this->GetDerivationName(
          target->GetName(), source->GetFullPath());
        nixFileStream << "      " << objDerivName << "\n";
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
  
  // Get library link flags for build phase - use vector for efficient concatenation
  std::vector<std::string> linkFlagsList;
  
  if (linkImpl) {
    // Reserve space for efficiency
    linkFlagsList.reserve(linkImpl->Libraries.size() + transitiveDeps.size());
    
    for (const cmLinkItem& item : linkImpl->Libraries) {
      if (item.Target && item.Target->IsImported()) {
        // This is an imported target from find_package
        std::string importedTargetName = item.Target->GetName();
        // Need to create target generator for package mapper access
        auto targetGen = cmNixTargetGenerator::New(target);
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
  }
  
  // Add transitive shared library dependencies to linkFlags (excluding direct ones)
  for (const std::string& depTarget : transitiveDeps) {
    if (directSharedDeps.find(depTarget) == directSharedDeps.end()) {
      std::string depDerivName = this->GetDerivationName(depTarget);
      linkFlagsList.push_back("${" + depDerivName + "}/" + this->GetLibraryPrefix() + 
                             depTarget + this->GetSharedLibraryExtension());
    }
  }
  
  // Join all link flags with spaces
  std::string linkFlags;
  if (!linkFlagsList.empty()) {
    linkFlags = " " + cmJoin(linkFlagsList, " ");
  }
  
  std::string linkCompilerCmd = this->GetCompilerCommand(primaryLang);
  
  // Build the buildPhase commands
  writer.StartMultilineString("buildPhase");
  
  if (target->GetType() == cmStateEnums::EXECUTABLE) {
    writer.WriteMultilineLine(linkCompilerCmd + " $objects" + linkFlags + " -o \"$out\"");
  } else if (target->GetType() == cmStateEnums::STATIC_LIBRARY) {
    writer.WriteMultilineLine("ar rcs \"$out\" $objects");
  } else if (target->GetType() == cmStateEnums::SHARED_LIBRARY) {
    // Get library version properties
    cmValue version = target->GetProperty("VERSION");
    cmValue soversion = target->GetProperty("SOVERSION");
    
    writer.WriteMultilineLine("mkdir -p $out");
    std::string libName = this->GetLibraryPrefix() + targetName + this->GetSharedLibraryExtension();
    
    if (version && soversion) {
      // Create versioned library and symlinks
      std::string versionedName = libName + "." + *version;
      std::string soversionName = libName + "." + *soversion;
      
      writer.WriteMultilineLine(linkCompilerCmd + " -shared $objects" + linkFlags +
                              " -Wl,-soname," + soversionName + " -Wl,-rpath,$out/lib -o $out/" + versionedName);
      writer.WriteMultilineLine("ln -sf " + versionedName + " $out/" + soversionName);
      writer.WriteMultilineLine("ln -sf " + versionedName + " $out/" + libName);
    } else {
      // Simple shared library without versioning
      writer.WriteMultilineLine(linkCompilerCmd + " -shared $objects" + linkFlags +
                              " -Wl,-rpath,$out/lib -o $out/" + libName);
    }
  } else if (target->GetType() == cmStateEnums::MODULE_LIBRARY) {
    // Module libraries are like shared libraries but without versioning or lib prefix
    writer.WriteMultilineLine("mkdir -p $out");
    std::string modName = targetName + this->GetSharedLibraryExtension();
    writer.WriteMultilineLine(linkCompilerCmd + " -shared $objects" + linkFlags +
                            " -o $out/" + modName);
  }
  
  writer.EndMultilineString();
  
  // For try_compile, copy the output file where CMake expects it for COPY_FILE
  if (isTryCompile) {
    if (this->GetCMakeInstance()->GetDebugOutput()) {
      std::cerr << "[NIX-TRACE] " << __FILE__ << ":" << __LINE__ 
                << " Adding try_compile output file handling for: " << targetName << std::endl;
    }
    
    writer.WriteComment("Handle try_compile COPY_FILE requirement");
    writer.StartMultilineString("postBuildPhase");
    writer.WriteMultilineLine("# Create output location in build directory for CMake COPY_FILE");
    writer.WriteMultilineLine("COPY_DEST=\"" + buildDir + "/" + targetName + "\"");
    writer.WriteMultilineLine("cp \"$out\" \"$COPY_DEST\"");
    if (this->GetCMakeInstance()->GetDebugOutput()) {
      writer.WriteMultilineLine("echo '[NIX-TRACE] Copied try_compile output to: '$COPY_DEST");
    }
    writer.WriteMultilineLine("# Write location file that CMake expects to find the executable path");
    writer.WriteMultilineLine("echo \"$COPY_DEST\" > \"" + buildDir + "/" + targetName + "_loc\"");
    if (this->GetCMakeInstance()->GetDebugOutput()) {
      writer.WriteMultilineLine("echo '[NIX-TRACE] Wrote location file: " + buildDir + "/" + targetName + "_loc'");
      writer.WriteMultilineLine("echo '[NIX-TRACE] Location file contains: '$COPY_DEST");
    }
    writer.EndMultilineString();
  }
  
  writer.WriteAttribute("installPhase", "true");
  writer.WriteComment("No install needed");
  writer.EndDerivation();
  writer.WriteLine();
}

std::vector<std::string> cmGlobalNixGenerator::GetSourceDependencies(
  std::string const& /*sourceFile*/) const
{
  // Header dependency tracking is implemented using compiler -MM flags
  return {};
}

std::string cmGlobalNixGenerator::GetCompilerPackage(const std::string& lang) const
{
  // Check cache first
  {
    std::lock_guard<std::mutex> lock(this->CacheMutex);
    auto it = this->CompilerPackageCache.find(lang);
    if (it != this->CompilerPackageCache.end()) {
      return it->second;
    }
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
    // Check for user-specified fallback package
    std::string fallbackVar = "CMAKE_NIX_" + lang + "_COMPILER_PACKAGE";
    cmValue fallbackPackage = cm->GetCacheDefinition(fallbackVar);
    if (fallbackPackage && !fallbackPackage->empty()) {
      result = *fallbackPackage;
    } else {
      // Default fallback packages
      if (lang == "Fortran") {
        result = "gcc"; // provides gfortran
      } else if (lang == "CUDA") {
        result = "cudatoolkit";
      } else if (lang == "Swift") {
        result = "swift";
      } else {
        result = "gcc"; // Default for C/CXX/ASM
      }
    }
  }

  if (cm->GetState()->GetGlobalPropertyAsBool("CMAKE_CROSSCOMPILING")) {
    result += "-cross";
  }
  
  // Cache the result
  {
    std::lock_guard<std::mutex> lock(this->CacheMutex);
    this->CompilerPackageCache[lang] = result;
  }
  return result;
}

std::string cmGlobalNixGenerator::GetCompilerCommand(const std::string& lang) const
{
  // Check cache first
  {
    std::lock_guard<std::mutex> lock(this->CacheMutex);
    auto it = this->CompilerCommandCache.find(lang);
    if (it != this->CompilerCommandCache.end()) {
      return it->second;
    }
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
    // Check for user-specified fallback command
    cmake* cm = this->GetCMakeInstance();
    std::string fallbackVar = "CMAKE_NIX_" + lang + "_COMPILER_COMMAND";
    cmValue fallbackCommand = cm->GetCacheDefinition(fallbackVar);
    if (fallbackCommand && !fallbackCommand->empty()) {
      result = *fallbackCommand;
    } else {
      // Default fallback commands
      if (lang == "Fortran") {
        result = "gfortran";
      } else if (lang == "CUDA") {
        result = "nvcc";
      } else if (lang == "Swift") {
        result = "swiftc";
      } else if (lang == "CXX") {
        result = "g++";
      } else {
        result = "gcc";
      }
    }
  }
  
  // Cache the result
  {
    std::lock_guard<std::mutex> lock(this->CacheMutex);
    this->CompilerCommandCache[lang] = result;
  }
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

std::vector<std::string> cmGlobalNixGenerator::GetCachedLibraryDependencies(
  cmGeneratorTarget* target, const std::string& config) const
{
  std::pair<cmGeneratorTarget*, std::string> cacheKey = {target, config};
  
  // Check cache first
  {
    std::lock_guard<std::mutex> lock(this->CacheMutex);
    auto it = this->LibraryDependencyCache.find(cacheKey);
    if (it != this->LibraryDependencyCache.end()) {
      return it->second;
    }
  }
  
  // Compute dependencies and cache them
  auto targetGen = cmNixTargetGenerator::New(target);
  std::vector<std::string> libraryDeps = targetGen->GetTargetLibraryDependencies(config);
  
  {
    std::lock_guard<std::mutex> lock(this->CacheMutex);
    this->LibraryDependencyCache[cacheKey] = libraryDeps;
  }
  
  return libraryDeps;
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