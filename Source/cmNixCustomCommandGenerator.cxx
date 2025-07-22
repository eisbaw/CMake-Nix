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
  nixFileStream << "    buildInputs = [ pkgs.coreutils ];\n";
  nixFileStream << "    phases = [ \"buildPhase\" ];\n";
  nixFileStream << "    buildPhase = ''\n";

  for (const auto& commandLine : this->CustomCommand->GetCommandLines()) {
    std::string fullCommand;
    for (const std::string& arg : commandLine) {
        if (!fullCommand.empty()) {
            fullCommand += " ";
        }
        fullCommand += "\"" + arg + "\"";
    }
    nixFileStream << "      " << fullCommand << "\n";
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
