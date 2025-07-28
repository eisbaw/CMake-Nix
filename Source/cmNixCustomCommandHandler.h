/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */
#pragma once

#include <memory>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

class cmCustomCommandGenerator;
class cmGeneratorTarget;
class cmLocalGenerator;
class cmNixCustomCommandGenerator;

/**
 * @brief Handles custom command processing for the Nix generator
 * 
 * This class encapsulates all custom command related functionality,
 * including collection, cycle detection, and delegation to the
 * custom command generator.
 */
class cmNixCustomCommandHandler
{
public:
  struct CustomCommandInfo {
    std::string Output;
    std::string TargetName;
    cmCustomCommandGenerator const* Generator;
    cmGeneratorTarget* Target;
    cmLocalGenerator* LocalGen;
    std::set<std::string> Depends;
    std::string DerivationName;
  };

  cmNixCustomCommandHandler();
  ~cmNixCustomCommandHandler();

  /**
   * @brief Collects all custom commands from the given local generators
   * @param localGenerators The local generators to scan
   * @return Map of output file to custom command info
   */
  std::unordered_map<std::string, CustomCommandInfo> 
    CollectCustomCommands(const std::vector<std::unique_ptr<cmLocalGenerator>>& localGenerators);

  /**
   * @brief Writes custom command derivations
   * @param customCommands The collected custom commands
   * @param customCommandOutputs Map of output files to derivation names
   * @param objectFileOutputs Map of object files to derivation names
   * @param fout Output stream to write to
   * @param projectSourceDir The project source directory
   * @param projectBinaryDir The project binary directory
   * @param debugOutput Whether to enable debug output
   */
  void WriteCustomCommandDerivations(
    const std::unordered_map<std::string, CustomCommandInfo>& customCommands,
    const std::map<std::string, std::string>* customCommandOutputs,
    const std::map<std::string, std::string>* objectFileOutputs,
    std::ostream& fout,
    const std::string& projectSourceDir,
    const std::string& projectBinaryDir,
    bool debugOutput);

  /**
   * @brief Detects cycles in custom command dependencies
   * @param customCommands The custom commands to check
   * @return True if a cycle is detected, false otherwise
   */
  bool DetectCustomCommandCycles(
    const std::unordered_map<std::string, CustomCommandInfo>& customCommands);

private:
  // Internal methods for cycle detection
  bool HasCycle(const std::unordered_map<std::string, CustomCommandInfo>& commands);
  
  bool DFSCycleCheck(
    const std::string& output,
    const std::unordered_map<std::string, CustomCommandInfo>& commands,
    std::set<std::string>& visited,
    std::set<std::string>& recursionStack);

  // Helper to collect custom commands from a single generator target
  void CollectFromTarget(
    cmGeneratorTarget* target,
    cmLocalGenerator* lg,
    std::unordered_map<std::string, CustomCommandInfo>& customCommands);
};