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
#include <set>

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
  nixFileStream << "    buildInputs = [ pkgs.coreutils pkgs.cmake";
  
  // Add dependencies on other custom commands
  std::vector<std::string> depends = this->GetDepends();
  for (const std::string& dep : depends) {
    // Check if this dependency is a custom command output
    std::string depDerivName = "custom_" + cmSystemTools::GetFilenameWithoutExtension(dep);
    nixFileStream << " " << depDerivName;
  }
  
  nixFileStream << "];\n";
  nixFileStream << "    phases = [ \"buildPhase\" ];\n";
  nixFileStream << "    buildPhase = ''\n";
  nixFileStream << "      mkdir -p $out\n";
  
  // Copy dependent files from other custom command outputs
  for (const std::string& dep : depends) {
    std::string depDerivName = "custom_" + cmSystemTools::GetFilenameWithoutExtension(dep);
    std::string depFile = cmSystemTools::GetFilenameName(dep);
    nixFileStream << "      cp ${" << depDerivName << "}/" << depFile << " .\n";
  }
  
  nixFileStream << "";

  // Execute commands with proper shell handling
  for (const auto& commandLine : this->CustomCommand->GetCommandLines()) {
    nixFileStream << "      ";
    bool first = true;
    for (const std::string& arg : commandLine) {
      if (!first) {
        nixFileStream << " ";
      }
      first = false;
      
      // Handle shell operators properly
      if (arg == ">" || arg == ">>" || arg == "<" || arg == "|" || arg == "&&" || arg == "||") {
        nixFileStream << arg;
      } else if (arg.find("/cmake") != std::string::npos && arg.find("/bin/cmake") != std::string::npos) {
        // Replace absolute cmake paths with just "cmake" from nixpkgs
        nixFileStream << "cmake";
      } else {
        // Quote arguments that aren't shell operators
        nixFileStream << "\"" << arg << "\"";
      }
    }
    nixFileStream << "\n";
  }

  // Copy outputs to derivation output
  for (const std::string& output : this->CustomCommand->GetOutputs()) {
    std::string outputFile = cmSystemTools::GetFilenameName(output);
    nixFileStream << "      cp " << outputFile << " $out/" << outputFile << "\n";
  }

  nixFileStream << "    '';\n";
  nixFileStream << "  };\n\n";
}

std::string cmNixCustomCommandGenerator::GetDerivationName() const
{
  // Create a unique name for the derivation based on the output file.
  // This assumes that the first output file is unique enough.
  return "custom_" + cmSystemTools::GetFilenameWithoutExtension(this->CustomCommand->GetOutputs()[0]);
}

std::vector<std::string> cmNixCustomCommandGenerator::GetOutputs() const
{
  return this->CustomCommand->GetOutputs();
}

std::vector<std::string> cmNixCustomCommandGenerator::GetDepends() const
{
  return this->CustomCommand->GetDepends();
}
