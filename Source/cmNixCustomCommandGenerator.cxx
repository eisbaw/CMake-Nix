/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */
#include "cmNixCustomCommandGenerator.h"

#include <sstream>
#include <algorithm>

#include "cmCustomCommand.h"
#include "cmCustomCommandGenerator.h"
#include "cmLocalGenerator.h"
#include "cmGeneratorExpression.h"
#include "cmSystemTools.h"
#include "cmMakefile.h"
#include "cmGeneratorTarget.h"

cmNixCustomCommandGenerator::cmNixCustomCommandGenerator(
  cmCustomCommand const* cc, cmLocalGenerator* lg, const std::string& config)
  : Command(cc)
  , LocalGen(lg)
  , Config(config)
{
}

void cmNixCustomCommandGenerator::Generate(std::ostream& os) const
{
  std::string derivName = this->GetDerivationName();
  os << "  " << derivName << " = stdenv.mkDerivation {\n";
  os << "    name = \"" << derivName << "\";\n";
  
  // Handle source directory  
  std::string sourceDir = this->LocalGen->GetMakefile()->GetHomeDirectory();
  std::string buildDir = this->LocalGen->GetMakefile()->GetHomeOutputDirectory();
  std::string srcPath = cmSystemTools::RelativePath(buildDir, sourceDir);
  if (srcPath.empty()) {
    srcPath = "./.";
  } else {
    // Remove trailing slash if present
    if (!srcPath.empty() && srcPath.back() == '/') {
      srcPath.pop_back();
    }
    srcPath = "./" + srcPath;
  }
  
  os << "    src = " << srcPath << ";\n";
  
  // Add dependencies
  std::vector<std::string> depends = this->GetDepends();
  os << "    buildInputs = [ cmake ];\n";
  if (!depends.empty()) {
    os << "    # Dependencies\n";
    os << "    deps = [\n";
    for (const auto& dep : depends) {
      os << "      " << this->MapToNixDep(dep) << "\n";
    }
    os << "    ];\n";
  }
  
  // Prevent CMake hook from running
  os << "    dontConfigure = true;\n";
  
  // Get working directory
  std::string workingDir = this->Command->GetWorkingDirectory();
  if (workingDir.empty()) {
    workingDir = this->LocalGen->GetCurrentBinaryDirectory();
  }
  
  // Convert to relative path from project root
  std::string projectRoot = this->LocalGen->GetMakefile()->GetHomeDirectory();
  std::string relWorkingDir = cmSystemTools::RelativePath(projectRoot, workingDir);
  
  // Generate build phase
  os << "    buildPhase = ''\n";
  os << "      # Custom command: " << this->Command->GetComment() << "\n";
  
  // Create output directory
  os << "      mkdir -p $out\n";
  
  // Change to working directory if needed
  if (!relWorkingDir.empty() && relWorkingDir != ".") {
    os << "      mkdir -p " << relWorkingDir << "\n";
    os << "      cd " << relWorkingDir << "\n";
  }
  
  // Execute commands
  std::vector<std::string> commandLines = this->GetCommandLines();
  for (const auto& cmdLine : commandLines) {
    os << "      " << cmdLine << "\n";
  }
  
  // Copy outputs to derivation output
  std::vector<std::string> outputs = this->GetOutputs();
  for (const auto& output : outputs) {
    std::string outputFile = cmSystemTools::GetFilenameName(output);
    os << "      cp " << outputFile << " $out/" << outputFile << " || true\n";
  }
  
  os << "    '';\n";
  os << "    installPhase = \"true\"; # No install needed\n";
  os << "  };\n\n";
}

std::string cmNixCustomCommandGenerator::GetDerivationName() const
{
  // Generate a name based on the first output
  std::vector<std::string> outputs = this->GetOutputs();
  if (!outputs.empty()) {
    std::string firstOutput = cmSystemTools::GetFilenameName(outputs[0]);
    std::replace(firstOutput.begin(), firstOutput.end(), '.', '_');
    return "custom_" + firstOutput;
  }
  
  // Fallback to generic name
  return "custom_command";
}

std::vector<std::string> cmNixCustomCommandGenerator::GetOutputs() const
{
  // Use the custom command generator to expand outputs
  cmCustomCommandGenerator ccg(*this->Command, this->Config, this->LocalGen);
  return ccg.GetOutputs();
}

std::vector<std::string> cmNixCustomCommandGenerator::GetDepends() const
{
  // Use the custom command generator to expand depends
  cmCustomCommandGenerator ccg(*this->Command, this->Config, this->LocalGen);
  return ccg.GetDepends();
}

bool cmNixCustomCommandGenerator::GeneratesOutput(const std::string& output) const
{
  std::vector<std::string> outputs = this->GetOutputs();
  return std::find(outputs.begin(), outputs.end(), output) != outputs.end();
}

std::string cmNixCustomCommandGenerator::EscapeForNix(const std::string& str) const
{
  std::string result = str;
  
  // Escape special characters for Nix strings
  size_t pos = 0;
  while ((pos = result.find('$', pos)) != std::string::npos) {
    if (pos + 1 < result.length() && result[pos + 1] == '{') {
      // This is a Nix interpolation, skip it
      pos += 2;
    } else {
      result.insert(pos, "\\");
      pos += 2;
    }
  }
  
  // Escape double quotes
  pos = 0;
  while ((pos = result.find('"', pos)) != std::string::npos) {
    result.insert(pos, "\\");
    pos += 2;
  }
  
  return result;
}

std::vector<std::string> cmNixCustomCommandGenerator::GetCommandLines() const
{
  std::vector<std::string> result;
  
  // Use custom command generator for proper expansion
  cmCustomCommandGenerator ccg(*this->Command, this->Config, this->LocalGen);
  
  // Process all commands (there can be multiple)
  for (unsigned int c = 0; c < ccg.GetNumberOfCommands(); ++c) {
    std::string cmd = ccg.GetCommand(c);
    
    // Replace absolute cmake paths with relative 'cmake' command
    // This allows the Nix cmake package to provide the command
    if (cmd.find("/cmake") != std::string::npos) {
      cmd = "cmake";
    }
    
    // Get arguments by appending them to the command
    std::string fullCmd = cmd;
    ccg.AppendArguments(c, fullCmd);
    
    result.push_back(fullCmd);
  }
  
  return result;
}

std::string cmNixCustomCommandGenerator::MapToNixDep(const std::string& dep) const
{
  // For now, just return the path relative to project root
  // In future, this could map to other derivations
  std::string projectRoot = this->LocalGen->GetMakefile()->GetHomeDirectory();
  std::string relPath = cmSystemTools::RelativePath(projectRoot, dep);
  
  if (!relPath.empty()) {
    return "./" + relPath;
  }
  return dep;
}