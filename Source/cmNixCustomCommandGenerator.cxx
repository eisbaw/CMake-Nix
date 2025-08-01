/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */
#include "cmNixCustomCommandGenerator.h"

#include "cmGlobalGenerator.h"
#include "cmCustomCommand.h"
#include "cmLocalGenerator.h"
#include "cmGeneratedFileStream.h"
#include "cmGeneratorTarget.h"
#include "cmMakefile.h"
#include "cmNixConstants.h"
#include "cmSystemTools.h"
#include "cmOutputConverter.h"
#include "cmake.h"
#include "cmNixPathUtils.h"
#include "cmCustomCommandGenerator.h"
#include <set>
#include <cctype>
#include <fstream>
#include <iostream>

cmNixCustomCommandGenerator::cmNixCustomCommandGenerator(cmCustomCommand const* cc, cmLocalGenerator* lg, std::string const& config,
                                                        const std::map<std::string, std::string>* customCommandOutputs,
                                                        const std::map<std::string, std::string>* objectFileOutputs)
  : CustomCommand(cc)
  , LocalGenerator(lg)
  , Config(config)
  , CustomCommandOutputs(customCommandOutputs)
  , ObjectFileOutputs(objectFileOutputs)
{
}

void cmNixCustomCommandGenerator::Generate(cmGeneratedFileStream& nixFileStream)
{
  // Create standard custom command generator to expand generator expressions
  cmCustomCommandGenerator ccGen(*this->CustomCommand, this->Config, this->LocalGenerator);
  
  nixFileStream << "  " << this->GetDerivationName() << " = stdenv.mkDerivation {\n";
  nixFileStream << "    name = \"" << this->GetDerivationName() << "\";\n";
  
  // Check if we need any build inputs
  bool needsCoreutils = false;
  bool hasNonEchoCommands = false;
  bool needsPython = false;
  bool needsCMake = false;
  bool needsSourceAccess = false;
  std::set<std::string> objectFileDeps;
  
  // Analyze commands to determine what tools we need
  // Use the expanded commands from ccGen
  for (unsigned int i = 0; i < ccGen.GetNumberOfCommands(); ++i) {
    std::string cmd = ccGen.GetCommand(i);
    if (!cmd.empty()) {
      // Check if this command uses Python
      if (cmd.find("/python") != std::string::npos || cmd.find("python3") != std::string::npos) {
        needsPython = true;
      }
      // Get full command line with arguments
      std::string fullCmd = cmd;
      ccGen.AppendArguments(i, fullCmd);
      // Check if any part references source files outside build directory
      if (fullCmd.find("/scripts/") != std::string::npos || 
          fullCmd.find("/cmake/") != std::string::npos) {
        needsSourceAccess = true;
      }
      // Check if command references object files
      std::vector<std::string> cmdArgs;
      cmSystemTools::ParseUnixCommandLine(fullCmd.c_str(), cmdArgs);
      for (const auto& arg : cmdArgs) {
        if (arg.find(".c.obj") != std::string::npos || 
            arg.find(".cpp.obj") != std::string::npos ||
            arg.find(".cxx.obj") != std::string::npos ||
            arg.find(cmNix::FilePatterns::OBJECT_FILE_SUFFIX) == arg.length() - strlen(cmNix::FilePatterns::OBJECT_FILE_SUFFIX)) {
          objectFileDeps.insert(arg);
        }
      }
      // Check if this is a cmake -E echo command
      if (cmd.find("cmake") != std::string::npos && fullCmd.find(" -E echo") != std::string::npos) {
        // Check if there's redirection
        if (fullCmd.find(" >") == std::string::npos && fullCmd.find(" >>") == std::string::npos) {
          continue;
        }
      }
      // Check if this command uses cmake
      if (cmd.find("/cmake") != std::string::npos && cmd.find(std::string(cmNix::SystemPaths::BIN) + "cmake") != std::string::npos) {
        needsCMake = true;
      }
      // Check if this command uses cmake -P (script mode)
      if (cmd.find("cmake") != std::string::npos && fullCmd.find(" -P ") != std::string::npos) {
        needsCMake = true;
        needsSourceAccess = true;  // We'll need the source tree for the script
        if (this->LocalGenerator->GetMakefile()->GetCMakeInstance()->GetDebugOutput()) {
          std::cerr << "[NIX-DEBUG] Detected cmake -P in custom command: " << fullCmd << std::endl;
        }
      }
      hasNonEchoCommands = true;
      needsCoreutils = true;
    }
  }
  
  // Only include necessary build inputs
  nixFileStream << "    buildInputs = [";
  bool firstInput = true;
  if (needsCoreutils) {
    nixFileStream << " pkgs.coreutils";
    firstInput = false;
  }
  if (needsPython) {
    if (!firstInput) nixFileStream << " ";
    nixFileStream << "pkgs.python3";
    firstInput = false;
  }
  if (needsCMake) {
    if (!firstInput) nixFileStream << " ";
    nixFileStream << "pkgs.cmake";
  }
  
  // Add dependencies on other custom commands and object files
  // Use expanded depends from ccGen
  std::vector<std::string> depends = ccGen.GetDepends();
  std::set<std::string> uniqueDepends;
  
  // Also need to track dependencies that will be referenced in the build phase
  // to ensure they're in buildInputs when we use ${derivationName}
  std::set<std::string> referencedDerivations;
  
  // Debug: log dependencies if debug output is enabled
  if (this->LocalGenerator->GetMakefile()->GetCMakeInstance()->GetDebugOutput()) {
    std::cerr << "[NIX-DEBUG] Custom command " << this->GetDerivationName() 
              << " has " << depends.size() << " dependencies:" << std::endl;
    for (const std::string& dep : depends) {
      std::cerr << "[NIX-DEBUG]   - " << dep << std::endl;
    }
  }
  
  // First pass: collect all derivations that will be referenced
  for (const std::string& dep : depends) {
    // Check if it's a custom command output
    if (this->CustomCommandOutputs) {
      auto it = this->CustomCommandOutputs->find(dep);
      if (it != this->CustomCommandOutputs->end()) {
        referencedDerivations.insert(it->second);
      }
    }
    
    // Check if it's an object file
    if (this->ObjectFileOutputs) {
      std::string depPath = dep;
      
      // Check if it's already a full path
      if (!cmSystemTools::FileIsFullPath(depPath)) {
        // Try relative to current binary directory first
        std::string fullPath = this->LocalGenerator->GetCurrentBinaryDirectory() + "/" + depPath;
        if (this->ObjectFileOutputs->find(fullPath) != this->ObjectFileOutputs->end()) {
          depPath = fullPath;
        } else {
          // Try relative to top-level binary directory
          depPath = this->LocalGenerator->GetGlobalGenerator()->GetCMakeInstance()->GetHomeOutputDirectory() + "/" + depPath;
        }
      }
      
      auto it = this->ObjectFileOutputs->find(depPath);
      if (it != this->ObjectFileOutputs->end()) {
        referencedDerivations.insert(it->second);
      }
    }
  }
  
  // Second pass: add all referenced derivations to buildInputs
  for (const std::string& derivName : referencedDerivations) {
    nixFileStream << " " << derivName;
  }
  
  nixFileStream << " ];\n";
  
  // If we need source access, add the source directory
  if (needsSourceAccess) {
    // For projects that need the source tree available
    std::string sourceDir = this->LocalGenerator->GetCurrentSourceDirectory();
    
    // Find the root source directory (where CMakeLists.txt with project() is)
    {
      while (!sourceDir.empty() && sourceDir != "/" && 
             !cmSystemTools::FileExists(sourceDir + "/CMakeLists.txt")) {
        sourceDir = cmSystemTools::GetParentDirectory(sourceDir);
      }
    }
    
    if (!sourceDir.empty() && sourceDir != "/") {
      nixFileStream << "    src = " << sourceDir << "/.;\n";
    }
  }
  
  // For custom commands, we don't need to copy the entire source tree
  // Only include the specific files we need
  if (hasNonEchoCommands) {
    if (needsSourceAccess) {
      // When we need source access, include unpackPhase to properly handle the src
      nixFileStream << "    phases = [ \"unpackPhase\" \"buildPhase\" ];\n";
    } else {
      nixFileStream << "    phases = [ \"buildPhase\" ];\n";
    }
    nixFileStream << "    buildPhase = ''\n";
    nixFileStream << "      mkdir -p $out\n";
    
    // If we have source access, we need to make the source tree available
    
    if (needsSourceAccess) {
      // The unpackPhase has already handled unpacking, and we should be in the source directory
      nixFileStream << "      # Source tree was unpacked by unpackPhase\n";
      nixFileStream << "      echo \"Current directory after unpack: $(pwd)\"\n";
      nixFileStream << "      echo \"Contents of current directory:\"\n";
      nixFileStream << "      ls -la | head -10\n";
      nixFileStream << "      echo \"Looking for cmake/gen_version_h.cmake:\"\n";
      nixFileStream << "      ls -la cmake/gen_version_h.cmake || echo \"File not found\"\n";
    }
    
    // Copy dependent files from other custom command outputs or configuration-time files
    for (const std::string& dep : depends) {
      // Check if this dependency is a custom command output
      bool isCustomCommandOutput = false;
      std::string depDerivName;
      if (this->CustomCommandOutputs) {
        auto it = this->CustomCommandOutputs->find(dep);
        if (it != this->CustomCommandOutputs->end()) {
          depDerivName = it->second;
          isCustomCommandOutput = true;
        }
      }
      
      // For multi-output custom commands, we need to use the relative path from the build directory
      std::string depPath = dep;
      if (cmSystemTools::FileIsFullPath(dep)) {
        // Use top-level build directory for consistent paths with how outputs are stored
        std::string topBuildDir = this->LocalGenerator->GetGlobalGenerator()->GetCMakeInstance()->GetHomeOutputDirectory();
        depPath = cmSystemTools::RelativePath(topBuildDir, dep);
      }
      
      if (isCustomCommandOutput) {
        // Copy from custom command output derivation
        // Create directory structure if needed
        std::string depDir = cmSystemTools::GetFilenamePath(depPath);
        if (!depDir.empty()) {
          nixFileStream << "      mkdir -p " << cmOutputConverter::EscapeForShell(depDir, cmOutputConverter::Shell_Flag_IsUnix) << "\n";
        }
        // SECURITY FIX: Escape the path to prevent shell injection
        // Copy to the proper location, not just current directory
        nixFileStream << "      cp ${" << depDerivName << "}/" 
                      << cmOutputConverter::EscapeForShell(depPath, cmOutputConverter::Shell_Flag_IsUnix) << " "
                      << cmOutputConverter::EscapeForShell(depPath, cmOutputConverter::Shell_Flag_IsUnix) << "\n";
      } else {
        // Check if it's an object file dependency
        bool isObjectFile = false;
        std::string objDerivName;
        if (this->ObjectFileOutputs) {
          std::string objPath = dep;
          if (!cmSystemTools::FileIsFullPath(objPath)) {
            objPath = this->LocalGenerator->GetGlobalGenerator()->GetCMakeInstance()->GetHomeOutputDirectory() + "/" + objPath;
          }
          auto objIt = this->ObjectFileOutputs->find(objPath);
          if (objIt != this->ObjectFileOutputs->end()) {
            objDerivName = objIt->second;
            isObjectFile = true;
          }
        }
        
        if (isObjectFile) {
          // Copy from object file derivation
          // Create directory structure if needed
          std::string depDir = cmSystemTools::GetFilenamePath(depPath);
          if (!depDir.empty()) {
            nixFileStream << "      mkdir -p " << cmOutputConverter::EscapeForShell(depDir, cmOutputConverter::Shell_Flag_IsUnix) << "\n";
          }
          // The object file derivation outputs to the root of $out
          // We need to copy it to the expected location
          nixFileStream << "      cp ${" << objDerivName << "}/* " 
                        << cmOutputConverter::EscapeForShell(depPath, cmOutputConverter::Shell_Flag_IsUnix) << " 2>/dev/null || cp ${" << objDerivName << "} " 
                        << cmOutputConverter::EscapeForShell(depPath, cmOutputConverter::Shell_Flag_IsUnix) << "\n";
        } else {
        // This is a configuration-time file or source file, not a custom command output
        // Check if it's a configuration-time generated file that needs to be embedded
        if (cmSystemTools::FileIsFullPath(dep)) {
          std::string buildDir = this->LocalGenerator->GetCurrentBinaryDirectory();
          std::string topBuildDir = this->LocalGenerator->GetGlobalGenerator()->GetCMakeInstance()->GetHomeOutputDirectory();
          std::string relToBuild = cmSystemTools::RelativePath(topBuildDir, dep);
          
          // If it's in the build directory and exists, embed it
          if (!cmNixPathUtils::IsPathOutsideTree(relToBuild) && cmSystemTools::FileExists(dep)) {
            // Read and embed the file content
            cmSystemTools::ConvertToUnixSlashes(relToBuild);
            std::string destDir = cmSystemTools::GetFilenamePath(relToBuild);
            if (!destDir.empty()) {
              nixFileStream << "      mkdir -p " << cmOutputConverter::EscapeForShell(destDir, cmOutputConverter::Shell_Flag_IsUnix) << "\n";
            }
            
            // Read the file and write it
            std::ifstream inFile(dep, std::ios::binary);
            if (inFile) {
              // Check file size first
              inFile.seekg(0, std::ios::end);
              std::streamsize fileSize = inFile.tellg();
              inFile.seekg(0, std::ios::beg);
              
              // Limit to 1MB for configuration-time embedded files
              const std::streamsize maxFileSize = 1024 * 1024; // 1MB
              if (fileSize > maxFileSize) {
                this->LocalGenerator->GetCMakeInstance()->IssueMessage(
                  MessageType::FATAL_ERROR,
                  cmStrCat("Configuration-time generated file '", dep, 
                          "' is too large to embed (", fileSize, " bytes). ",
                          "Maximum allowed size is ", maxFileSize, " bytes (1MB). ",
                          "Consider using a different approach for large generated files."),
                  cmListFileBacktrace());
                return;
              }
              
              std::string content((std::istreambuf_iterator<char>(inFile)), 
                                  std::istreambuf_iterator<char>());
              // Ensure content ends with a newline to avoid here-doc issues
              if (!content.empty() && content.back() != '\n') {
                content += '\n';
              }
              
              // Escape '' sequences in content since we're inside a Nix multiline string
              std::string escapedContent;
              for (size_t i = 0; i < content.length(); ++i) {
                if (i + 1 < content.length() && content[i] == '\'' && content[i + 1] == '\'') {
                  escapedContent += "''\\''";
                  i++; // Skip the next quote
                } else {
                  escapedContent += content[i];
                }
              }
              
              // Use unique delimiter to avoid conflicts with file content
              std::string delimiter = "EOF_" + std::to_string(std::hash<std::string>{}(dep) % 1000000);
              nixFileStream << "      cat > " << cmOutputConverter::EscapeForShell(relToBuild, cmOutputConverter::Shell_Flag_IsUnix) 
                           << " <<'" << delimiter << "'\n" << escapedContent << delimiter << "\n";
            }
          }
        }
        }
      }
    }
    
    // Execute commands with proper shell handling
    // Use the expanded commands from ccGen
    for (unsigned int i = 0; i < ccGen.GetNumberOfCommands(); ++i) {
      std::string cmd = ccGen.GetCommand(i);
      if (cmd.empty()) continue;
      
      // Get full command with arguments
      std::string fullCmd = cmd;
      ccGen.AppendArguments(i, fullCmd);
      
      // Check if this is a cmake -E echo command without redirection
      if (cmd.find("cmake") != std::string::npos && fullCmd.find(" -E echo") != std::string::npos) {
        if (fullCmd.find(" >") == std::string::npos && fullCmd.find(" >>") == std::string::npos) {
          continue;
        }
      }
      
      nixFileStream << "      ";
      
      // Process the command
      std::string processedCmd = cmd;
      
      // Replace absolute python paths with Nix-provided Python
      if (cmd.find("/python") != std::string::npos && cmd.find(std::string(cmNix::SystemPaths::BIN) + "python") != std::string::npos) {
        processedCmd = "python3";
      } 
      // Replace absolute cmake paths
      else if (cmd.find("/cmake") != std::string::npos && cmd.find(std::string(cmNix::SystemPaths::BIN) + "cmake") != std::string::npos) {
        // Check if this is a -E echo command
        if (fullCmd.find(" -E echo") != std::string::npos) {
          // Extract everything after "-E echo" from fullCmd
          size_t echoPos = fullCmd.find(" -E echo");
          if (echoPos != std::string::npos) {
            size_t contentStart = fullCmd.find_first_not_of(" ", echoPos + 8);
            if (contentStart != std::string::npos) {
              nixFileStream << "echo " << fullCmd.substr(contentStart) << "\n";
              continue;
            }
          }
        } else {
          // For other cmake commands, use cmake from nixpkgs
          processedCmd = "${pkgs.cmake}" + std::string(cmNix::SystemPaths::BIN) + "cmake";
        }
      }
      // Handle source paths if needed
      else if (needsSourceAccess && cmSystemTools::FileIsFullPath(cmd)) {
        std::string sourceDir = this->LocalGenerator->GetCurrentSourceDirectory();
        while (!sourceDir.empty() && sourceDir != "/" && 
               !cmSystemTools::FileExists(sourceDir + "/CMakeLists.txt")) {
          sourceDir = cmSystemTools::GetParentDirectory(sourceDir);
        }
        
        if (!sourceDir.empty() && cmd.find(sourceDir) == 0) {
          processedCmd = cmSystemTools::RelativePath(sourceDir, cmd);
        }
      }
      
      // Write the processed command
      nixFileStream << processedCmd;
      
      // Now handle arguments
      std::string args;
      ccGen.AppendArguments(i, args);
      
      // Reconstruct the full command for parsing
      std::string fullCmdForParsing = processedCmd + " " + args;
      
      // Process arguments for output paths and source paths
      std::vector<std::string> argv;
      cmSystemTools::ParseUnixCommandLine(fullCmdForParsing.c_str(), argv);
      
      // Skip the first element (command) and process the rest
      bool nextArgIsScript = false;
      for (size_t j = 1; j < argv.size(); ++j) {
        std::string arg = argv[j];
        nixFileStream << " ";
        
        // Check if previous arg was -P (cmake script argument)
        if (nextArgIsScript && !cmSystemTools::FileIsFullPath(arg)) {
          // This is a relative path to a CMake script that needs to be resolved
          // In the Nix build environment, we need to use the absolute path
          // Script paths need to be kept relative for the Nix build environment
          // The source tree will be made available via needsSourceAccess
          nextArgIsScript = false;
        } else if (arg == "-P") {
          nextArgIsScript = true;
        }
        
        // Check if this is an object file dependency that needs translation
        if (arg.find(".c.obj") != std::string::npos) {
          // Replace .c.obj with .o
          size_t pos = arg.rfind(".c.obj");
          if (pos != std::string::npos) {
            arg = arg.substr(0, pos) + cmNix::FilePatterns::OBJECT_FILE_SUFFIX;
          }
        }
        
        bool isOutputPath = false;
        bool isSourcePath = false;
        std::string relativeArg = arg;
        
        // Check if it's an output path
        for (const std::string& output : ccGen.GetOutputs()) {
          if (arg == output && cmSystemTools::FileIsFullPath(arg)) {
            std::string buildDir = this->LocalGenerator->GetGlobalGenerator()->GetCMakeInstance()->GetHomeOutputDirectory();
            relativeArg = cmSystemTools::RelativePath(buildDir, arg);
            isOutputPath = true;
            break;
          }
        }
        
        // Check if it's a source path
        if (!isOutputPath && needsSourceAccess && cmSystemTools::FileIsFullPath(arg)) {
          std::string sourceDir = this->LocalGenerator->GetCurrentSourceDirectory();
          while (!sourceDir.empty() && sourceDir != "/" && 
                 !cmSystemTools::FileExists(sourceDir + "/CMakeLists.txt")) {
            sourceDir = cmSystemTools::GetParentDirectory(sourceDir);
          }
          
          if (!sourceDir.empty() && arg.find(sourceDir) == 0) {
            relativeArg = cmSystemTools::RelativePath(sourceDir, arg);
            isSourcePath = true;
          }
        }
        
        // Shell operators don't need escaping
        if (arg == ">" || arg == ">>" || arg == "<" || arg == "|" || arg == "&&" || arg == "||" || arg == ";" || arg == "&") {
          nixFileStream << arg;
        } else if (arg.find("$UNPACKED_SOURCE_DIR/") == 0) {
          // Don't escape arguments that contain shell variables
          nixFileStream << arg;
        } else {
          nixFileStream << cmOutputConverter::EscapeForShell((isOutputPath || isSourcePath) ? relativeArg : arg, cmOutputConverter::Shell_Flag_IsUnix);
        }
      }
      nixFileStream << "\n";
    }

    // Copy outputs to derivation output, preserving directory structure
    for (const std::string& output : ccGen.GetOutputs()) {
      // Get relative path from top-level build directory to preserve structure
      // This ensures consistent paths when files are referenced from other directories
      std::string buildDir = this->LocalGenerator->GetGlobalGenerator()->GetCMakeInstance()->GetHomeOutputDirectory();
      std::string relativePath = cmSystemTools::RelativePath(buildDir, output);
      
      // Create directory structure if needed
      std::string outputDir = cmSystemTools::GetFilenamePath(relativePath);
      if (!outputDir.empty()) {
        nixFileStream << "      mkdir -p $out/" << cmOutputConverter::EscapeForShell(outputDir, cmOutputConverter::Shell_Flag_IsUnix) << "\n";
      }
      
      // Copy file to proper location
      // If we cd'd to the unpacked source directory, the output might be there
      nixFileStream << "      if [ -f " << cmOutputConverter::EscapeForShell(relativePath, cmOutputConverter::Shell_Flag_IsUnix) << " ]; then\n";
      nixFileStream << "        cp " << cmOutputConverter::EscapeForShell(relativePath, cmOutputConverter::Shell_Flag_IsUnix) 
                    << " $out/" << cmOutputConverter::EscapeForShell(relativePath, cmOutputConverter::Shell_Flag_IsUnix) << "\n";
      nixFileStream << "      elif [ -f " << cmOutputConverter::EscapeForShell(cmSystemTools::GetFilenameName(output), cmOutputConverter::Shell_Flag_IsUnix) << " ]; then\n";
      nixFileStream << "        cp " << cmOutputConverter::EscapeForShell(cmSystemTools::GetFilenameName(output), cmOutputConverter::Shell_Flag_IsUnix) 
                    << " $out/" << cmOutputConverter::EscapeForShell(relativePath, cmOutputConverter::Shell_Flag_IsUnix) << "\n";
      nixFileStream << "      fi\n";
    }

    nixFileStream << "    '';\n";
  } else {
    // If all commands were just echo commands, create an empty output
    nixFileStream << "    phases = [ \"installPhase\" ];\n";
    nixFileStream << "    installPhase = ''\n";
    nixFileStream << "      mkdir -p $out\n";
    for (const std::string& output : ccGen.GetOutputs()) {
      // Get relative path from top-level build directory to preserve structure
      // This ensures consistent paths when files are referenced from other directories
      std::string buildDir = this->LocalGenerator->GetGlobalGenerator()->GetCMakeInstance()->GetHomeOutputDirectory();
      std::string relativePath = cmSystemTools::RelativePath(buildDir, output);
      
      // Create directory structure if needed
      std::string outputDir = cmSystemTools::GetFilenamePath(relativePath);
      if (!outputDir.empty()) {
        nixFileStream << "      mkdir -p $out/" << cmOutputConverter::EscapeForShell(outputDir, cmOutputConverter::Shell_Flag_IsUnix) << "\n";
      }
      
      // Create empty file at proper location
      nixFileStream << "      touch $out/" << cmOutputConverter::EscapeForShell(relativePath, cmOutputConverter::Shell_Flag_IsUnix) << "\n";
    }
    nixFileStream << "    '';\n";
  }
  
  nixFileStream << "  };\n\n";
}

std::string cmNixCustomCommandGenerator::GetDerivationName() const
{
  // Create a unique name for the derivation based on the output file.
  // Include more of the path to avoid collisions in large projects
  const std::vector<std::string>& outputs = this->CustomCommand->GetOutputs();
  if (outputs.empty()) {
    // Return a fallback name if no outputs (shouldn't happen in valid CMake)
    return "custom_command_no_output";
  }
  
  // Use the first output as the base for the name
  std::string firstOutput = outputs[0];
  std::string baseName = this->GetDerivationNameForPath(firstOutput);
  
  // If there are multiple outputs, append a hash of all outputs to ensure uniqueness
  if (outputs.size() > 1) {
    // Create a simple hash from all outputs
    size_t hash = 0;
    for (const auto& output : outputs) {
      hash ^= std::hash<std::string>{}(output) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }
    baseName += "_" + std::to_string(hash % HASH_SUFFIX_DIGITS); // Add hash suffix (0-9999)
  }
  
  // Also add hash of the command itself to ensure different commands with same output get unique names
  if (!this->CustomCommand->GetCommandLines().empty()) {
    size_t cmdHash = 0;
    for (const auto& cmdLine : this->CustomCommand->GetCommandLines()) {
      for (const auto& arg : cmdLine) {
        cmdHash ^= std::hash<std::string>{}(arg) + 0x9e3779b9 + (cmdHash << 6) + (cmdHash >> 2);
      }
    }
    baseName += "_" + std::to_string(cmdHash % HASH_SUFFIX_DIGITS);
  }
  
  return baseName;
}

std::string cmNixCustomCommandGenerator::GetDerivationNameForPath(const std::string& path) const
{
  // Create a sanitized derivation name from a path
  // Replace directory separators and special chars with underscores
  std::string result = "custom_";
  
  // Get relative path if possible for better names
  std::string cleanPath = path;
  if (cmSystemTools::FileIsFullPath(cleanPath)) {
    cleanPath = cmSystemTools::RelativePath(
      this->LocalGenerator->GetCurrentSourceDirectory(), cleanPath);
  }
  
  // Replace problematic characters
  for (char c : cleanPath) {
    if (c == '/' || c == '\\' || c == '.' || c == '-' || c == ' ') {
      result += '_';
    } else if (std::isalnum(c)) {
      result += c;
    }
  }
  
  return result;
}

const std::vector<std::string>& cmNixCustomCommandGenerator::GetOutputs() const
{
  return this->CustomCommand->GetOutputs();
}

const std::vector<std::string>& cmNixCustomCommandGenerator::GetDepends() const
{
  return this->CustomCommand->GetDepends();
}
