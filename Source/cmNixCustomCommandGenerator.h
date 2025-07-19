/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */
#pragma once

#include "cmConfigure.h" // IWYU pragma: keep

#include <string>
#include <vector>
#include <ostream>

class cmCustomCommand;
class cmLocalGenerator;
class cmGeneratorTarget;

class cmNixCustomCommandGenerator
{
public:
  cmNixCustomCommandGenerator(cmCustomCommand const* cc,
                             cmLocalGenerator* lg,
                             const std::string& config);

  /// Generate Nix derivation for this custom command
  void Generate(std::ostream& os) const;

  /// Get the derivation name for this custom command
  std::string GetDerivationName() const;

  /// Get outputs of this custom command
  std::vector<std::string> GetOutputs() const;

  /// Get dependencies of this custom command
  std::vector<std::string> GetDepends() const;

  /// Check if this command generates the given output
  bool GeneratesOutput(const std::string& output) const;

private:
  /// Escape string for Nix
  std::string EscapeForNix(const std::string& str) const;

  /// Expand generator expressions
  std::string ExpandGeneratorExpressions(const std::string& input) const;

  /// Get command lines with proper expansion
  std::vector<std::string> GetCommandLines() const;

  /// Convert dependency to Nix reference
  std::string MapToNixDep(const std::string& dep) const;

private:
  cmCustomCommand const* Command;
  cmLocalGenerator* LocalGen;
  std::string Config;
};