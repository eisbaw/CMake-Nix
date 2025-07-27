/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */
#include "cmNixCustomCommandGenerator.h"

#include "cmGlobalGenerator.h"
#include "cmCustomCommand.h"
#include "cmLocalGenerator.h"
#include "cmGeneratedFileStream.h"
#include "cmGeneratorTarget.h"
#include "cmMakefile.h"
#include "cmSystemTools.h"
#include "cmOutputConverter.h"
#include "cmake.h"
#include "cmNixPathUtils.h"
#include "cmCustomCommandGenerator.h"
#include <set>
#include <cctype>
#include <fstream>

cmNixCustomCommandGenerator::cmNixCustomCommandGenerator(cmCustomCommand const* cc, cmLocalGenerator* lg, std::string const& config,
                                                        const std::map<std::string, std::string>* customCommandOutputs)
  : CustomCommand(cc)
  , LocalGenerator(lg)
  , Config(config)
  , CustomCommandOutputs(customCommandOutputs)
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
  bool needsSourceAccess = false;
  
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
          fullCmd.find("/zephyr/") != std::string::npos ||
          fullCmd.find("/cmake/") != std::string::npos) {
        needsSourceAccess = true;
      }
      // Check if this is a cmake -E echo command
      if (cmd.find("cmake") != std::string::npos && fullCmd.find(" -E echo") != std::string::npos) {
        // Check if there's redirection
        if (fullCmd.find(" >") == std::string::npos && fullCmd.find(" >>") == std::string::npos) {
          continue;
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
  }
  
  // Add dependencies on other custom commands
  // Use expanded depends from ccGen
  std::vector<std::string> depends = ccGen.GetDepends();
  std::set<std::string> uniqueDepends;
  for (const std::string& dep : depends) {
    // Only include custom command outputs as buildInputs
    if (this->CustomCommandOutputs) {
      auto it = this->CustomCommandOutputs->find(dep);
      if (it != this->CustomCommandOutputs->end()) {
        if (uniqueDepends.insert(it->second).second) {
          nixFileStream << " " << it->second;
        }
      }
      // If it's not a custom command output, it's a configuration-time file
      // and doesn't need to be in buildInputs
    }
  }
  
  nixFileStream << " ];\n";
  
  // If we need source access, add the source directory
  if (needsSourceAccess) {
    // For Zephyr RTOS and similar projects, we need to make the source tree available
    std::string sourceDir = this->LocalGenerator->GetCurrentSourceDirectory();
    // Find the root source directory (where CMakeLists.txt with project() is)
    while (!sourceDir.empty() && sourceDir != "/" && 
           !cmSystemTools::FileExists(sourceDir + "/CMakeLists.txt")) {
      sourceDir = cmSystemTools::GetParentDirectory(sourceDir);
    }
    if (!sourceDir.empty() && sourceDir != "/") {
      nixFileStream << "    src = " << sourceDir << ";\n";
    }
  }
  
  // For custom commands, we don't need to copy the entire source tree
  // Only include the specific files we need
  if (hasNonEchoCommands) {
    nixFileStream << "    phases = [ \"buildPhase\" ];\n";
    nixFileStream << "    buildPhase = ''\n";
    nixFileStream << "      mkdir -p $out\n";
    
    // If we have source access, we need to change to the source directory
    if (needsSourceAccess) {
      nixFileStream << "      # Change to the source directory unpacked by Nix\n";
      nixFileStream << "      cd /build/*\n";
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
        std::string buildDir = this->LocalGenerator->GetCurrentBinaryDirectory();
        depPath = cmSystemTools::RelativePath(buildDir, dep);
      }
      
      if (isCustomCommandOutput) {
        // Copy from custom command output derivation
        // SECURITY FIX: Escape the path to prevent shell injection
        nixFileStream << "      cp ${" << depDerivName << "}/" 
                      << cmOutputConverter::EscapeForShell(depPath, cmOutputConverter::Shell_Flag_IsUnix) << " .\n";
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
              std::string content((std::istreambuf_iterator<char>(inFile)), 
                                  std::istreambuf_iterator<char>());
              nixFileStream << "      cat > " << cmOutputConverter::EscapeForShell(relToBuild, cmOutputConverter::Shell_Flag_IsUnix) 
                           << " <<'EOF'\n" << content << "\nEOF\n";
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
      if (cmd.find("/python") != std::string::npos && cmd.find("/bin/python") != std::string::npos) {
        processedCmd = "python3";
      } 
      // Replace absolute cmake paths with echo for -E echo commands
      else if (cmd.find("/cmake") != std::string::npos && cmd.find("/bin/cmake") != std::string::npos && 
               fullCmd.find(" -E echo") != std::string::npos) {
        // Extract everything after "-E echo" from fullCmd
        size_t echoPos = fullCmd.find(" -E echo");
        if (echoPos != std::string::npos) {
          size_t contentStart = fullCmd.find_first_not_of(" ", echoPos + 8);
          if (contentStart != std::string::npos) {
            nixFileStream << "echo " << fullCmd.substr(contentStart) << "\n";
            continue;
          }
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
      
      // Process arguments for output paths and source paths
      std::vector<std::string> argv;
      cmSystemTools::ParseUnixCommandLine(fullCmd.c_str(), argv);
      
      // Skip the first element (command) and process the rest
      for (size_t j = 1; j < argv.size(); ++j) {
        const std::string& arg = argv[j];
        nixFileStream << " ";
        
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
      
      // Copy file to proper location - the file should be at the relative path we used in the command
      nixFileStream << "      cp " << cmOutputConverter::EscapeForShell(relativePath, cmOutputConverter::Shell_Flag_IsUnix) 
                    << " $out/" << cmOutputConverter::EscapeForShell(relativePath, cmOutputConverter::Shell_Flag_IsUnix) << "\n";
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
  // Include more of the path to avoid collisions in large projects like Zephyr
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
    baseName += "_" + std::to_string(hash % HASH_SUFFIX_DIGITS); // Add last 4 digits of hash
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
