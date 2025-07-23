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
#include <set>
#include <cctype>

cmNixCustomCommandGenerator::cmNixCustomCommandGenerator(cmCustomCommand const* cc, cmLocalGenerator* lg, std::string const& config)
  : CustomCommand(cc)
  , LocalGenerator(lg)
  , Config(config)
{
}

void cmNixCustomCommandGenerator::Generate(cmGeneratedFileStream& nixFileStream)
{
  nixFileStream << "  " << this->GetDerivationName() << " = stdenv.mkDerivation {\n";
  nixFileStream << "    name = \"" << this->GetDerivationName() << "\";\n";
  
  // Check if we need any build inputs
  bool needsCoreutils = false;
  bool hasNonEchoCommands = false;
  
  // Analyze commands to determine what tools we need
  for (const auto& commandLine : this->CustomCommand->GetCommandLines()) {
    if (!commandLine.empty()) {
      const std::string& cmd = commandLine[0];
      // Check if this is a cmake -E echo command with redirection
      if (commandLine.size() >= 3 && cmd.find("cmake") != std::string::npos && 
          commandLine[1] == "-E" && commandLine[2] == "echo") {
        // Only skip if there's no shell redirection
        bool hasRedirection = false;
        for (const auto& arg : commandLine) {
          if (arg == ">" || arg == ">>") {
            hasRedirection = true;
            break;
          }
        }
        if (!hasRedirection) {
          continue;
        }
      }
      hasNonEchoCommands = true;
      needsCoreutils = true;
    }
  }
  
  // Only include necessary build inputs
  nixFileStream << "    buildInputs = [";
  if (needsCoreutils) {
    nixFileStream << " pkgs.coreutils";
  }
  
  // Add dependencies on other custom commands
  std::vector<std::string> depends = this->GetDepends();
  std::set<std::string> uniqueDepends;
  for (const std::string& dep : depends) {
    // Generate a unique derivation name for this dependency
    std::string depDerivName = this->GetDerivationNameForPath(dep);
    if (uniqueDepends.insert(depDerivName).second) {
      nixFileStream << " " << depDerivName;
    }
  }
  
  nixFileStream << " ];\n";
  
  // For custom commands, we don't need to copy the entire source tree
  // Only include the specific files we need
  if (hasNonEchoCommands) {
    nixFileStream << "    phases = [ \"buildPhase\" ];\n";
    nixFileStream << "    buildPhase = ''\n";
    nixFileStream << "      mkdir -p $out\n";
    
    // Copy dependent files from other custom command outputs
    for (const std::string& dep : depends) {
      std::string depDerivName = this->GetDerivationNameForPath(dep);
      std::string depFile = cmSystemTools::GetFilenameName(dep);
      nixFileStream << "      cp ${" << depDerivName << "}/" 
                    << cmOutputConverter::EscapeForShell(depFile, cmOutputConverter::Shell_Flag_IsUnix) << " .\n";
    }
    
    // Execute commands with proper shell handling
    for (const auto& commandLine : this->CustomCommand->GetCommandLines()) {
      if (commandLine.empty()) continue;
      
      // Check if this is a cmake -E echo command with redirection
      if (commandLine.size() >= 3 && commandLine[0].find("cmake") != std::string::npos && 
          commandLine[1] == "-E" && commandLine[2] == "echo") {
        // Only skip if there's no shell redirection
        bool hasRedirection = false;
        for (const auto& arg : commandLine) {
          if (arg == ">" || arg == ">>") {
            hasRedirection = true;
            break;
          }
        }
        if (!hasRedirection) {
          continue;
        }
      }
      
      nixFileStream << "      ";
      bool first = true;
      int skipNext = 0;
      for (size_t i = 0; i < commandLine.size(); ++i) {
        const std::string& arg = commandLine[i];
        
        // Skip -E and echo arguments after cmake command
        if (skipNext > 0) {
          skipNext--;
          continue;
        }
        
        if (!first) {
          nixFileStream << " ";
        }
        first = false;
        
        // Handle shell operators properly
        if (arg == ">" || arg == ">>" || arg == "<" || arg == "|" || arg == "&&" || arg == "||") {
          nixFileStream << arg;
        } else if (arg.find("/cmake") != std::string::npos && arg.find("/bin/cmake") != std::string::npos) {
          // Replace absolute cmake paths with echo for -E echo commands
          if (commandLine.size() >= 3 && commandLine[1] == "-E" && commandLine[2] == "echo") {
            nixFileStream << "echo";
            skipNext = 2; // Skip -E and echo
          } else {
            // For other cmake commands, we might need the actual cmake
            // Properly escape to prevent shell injection
            nixFileStream << cmOutputConverter::EscapeForShell(arg, cmOutputConverter::Shell_Flag_IsUnix);
          }
        } else {
          // Properly escape arguments for shell
          nixFileStream << cmOutputConverter::EscapeForShell(arg, cmOutputConverter::Shell_Flag_IsUnix);
        }
      }
      nixFileStream << "\n";
    }

    // Copy outputs to derivation output
    for (const std::string& output : this->CustomCommand->GetOutputs()) {
      std::string outputFile = cmSystemTools::GetFilenameName(output);
      std::string escapedOutputFile = cmOutputConverter::EscapeForShell(outputFile, cmOutputConverter::Shell_Flag_IsUnix);
      nixFileStream << "      cp " << escapedOutputFile << " $out/" << escapedOutputFile << "\n";
    }

    nixFileStream << "    '';\n";
  } else {
    // If all commands were just echo commands, create an empty output
    nixFileStream << "    phases = [ \"installPhase\" ];\n";
    nixFileStream << "    installPhase = ''\n";
    nixFileStream << "      mkdir -p $out\n";
    for (const std::string& output : this->CustomCommand->GetOutputs()) {
      std::string outputFile = cmSystemTools::GetFilenameName(output);
      std::string escapedOutputFile = cmOutputConverter::EscapeForShell(outputFile, cmOutputConverter::Shell_Flag_IsUnix);
      nixFileStream << "      touch $out/" << escapedOutputFile << "\n";
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
  std::string firstOutput = outputs[0];
  return this->GetDerivationNameForPath(firstOutput);
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

std::vector<std::string> cmNixCustomCommandGenerator::GetOutputs() const
{
  return this->CustomCommand->GetOutputs();
}

std::vector<std::string> cmNixCustomCommandGenerator::GetDepends() const
{
  return this->CustomCommand->GetDepends();
}
