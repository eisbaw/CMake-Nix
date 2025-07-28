/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */
#include "cmNixCustomCommandHandler.h"

#include <algorithm>
#include <iostream>

#include "cmCustomCommandGenerator.h"
#include "cmGeneratorTarget.h"
#include "cmGlobalGenerator.h"
#include "cmLocalGenerator.h"
#include "cmMakefile.h"
#include "cmNixCustomCommandGenerator.h"
#include "cmSourceFile.h"
#include "cmSystemTools.h"
#include "cmGeneratedFileStream.h"
#include "cmCustomCommand.h"

cmNixCustomCommandHandler::cmNixCustomCommandHandler() = default;
cmNixCustomCommandHandler::~cmNixCustomCommandHandler() = default;

std::unordered_map<std::string, cmNixCustomCommandHandler::CustomCommandInfo>
cmNixCustomCommandHandler::CollectCustomCommands(
  const std::vector<std::unique_ptr<cmLocalGenerator>>& localGenerators)
{
  std::unordered_map<std::string, CustomCommandInfo> customCommands;
  
  for (auto const& lg : localGenerators) {
    // Process each target in this local generator
    auto const& targets = lg->GetGeneratorTargets();
    for (auto const& target : targets) {
      this->CollectFromTarget(target.get(), lg.get(), customCommands);
    }
  }
  
  return customCommands;
}

void cmNixCustomCommandHandler::CollectFromTarget(
  cmGeneratorTarget* target,
  cmLocalGenerator* lg,
  std::unordered_map<std::string, CustomCommandInfo>& customCommands)
{
  // Process pre-build commands
  for (const auto& cc : target->GetPreBuildCommands()) {
    auto ccg = std::make_unique<cmCustomCommandGenerator>(cc, "Release", lg);
    
    // Get the outputs
    std::vector<std::string> outputs = ccg->GetOutputs();
    if (outputs.empty()) {
      continue;
    }
    
    // Create command info for the primary output
    std::string primaryOutput = outputs[0];
    CustomCommandInfo info;
    info.Output = primaryOutput;
    info.TargetName = target->GetName();
    info.Generator = ccg.release();
    info.Target = target;
    info.LocalGen = lg;
    
    // Collect dependencies
    std::vector<std::string> depends = info.Generator->GetDepends();
    for (const auto& dep : depends) {
      info.Depends.insert(dep);
    }
    
    // Get the derivation name from the actual generator
    // This ensures consistency with what will be written to the Nix file
    const cmCustomCommand& cmd = info.Generator->GetCC();
    cmNixCustomCommandGenerator nixGen(&cmd, lg, "Release", nullptr, nullptr);
    info.DerivationName = nixGen.GetDerivationName();
    
    customCommands[primaryOutput] = std::move(info);
  }
  
  // Process pre-link commands
  for (const auto& cc : target->GetPreLinkCommands()) {
    auto ccg = std::make_unique<cmCustomCommandGenerator>(cc, "Release", lg);
    
    // Get the outputs
    std::vector<std::string> outputs = ccg->GetOutputs();
    if (outputs.empty()) {
      continue;
    }
    
    // Create command info for the primary output
    std::string primaryOutput = outputs[0];
    CustomCommandInfo info;
    info.Output = primaryOutput;
    info.TargetName = target->GetName();
    info.Generator = ccg.release();
    info.Target = target;
    info.LocalGen = lg;
    
    // Collect dependencies
    std::vector<std::string> depends = info.Generator->GetDepends();
    for (const auto& dep : depends) {
      info.Depends.insert(dep);
    }
    
    // Get the derivation name from the actual generator
    // This ensures consistency with what will be written to the Nix file
    const cmCustomCommand& cmd = info.Generator->GetCC();
    cmNixCustomCommandGenerator nixGen(&cmd, lg, "Release", nullptr, nullptr);
    info.DerivationName = nixGen.GetDerivationName();
    
    customCommands[primaryOutput] = std::move(info);
  }
  
  // Process post-build commands
  for (const auto& cc : target->GetPostBuildCommands()) {
    auto ccg = std::make_unique<cmCustomCommandGenerator>(cc, "Release", lg);
    
    // Get the outputs
    std::vector<std::string> outputs = ccg->GetOutputs();
    if (outputs.empty()) {
      continue;
    }
    
    // Create command info for the primary output
    std::string primaryOutput = outputs[0];
    CustomCommandInfo info;
    info.Output = primaryOutput;
    info.TargetName = target->GetName();
    info.Generator = ccg.release();
    info.Target = target;
    info.LocalGen = lg;
    
    // Collect dependencies
    std::vector<std::string> depends = info.Generator->GetDepends();
    for (const auto& dep : depends) {
      info.Depends.insert(dep);
    }
    
    // Get the derivation name from the actual generator
    // This ensures consistency with what will be written to the Nix file
    const cmCustomCommand& cmd = info.Generator->GetCC();
    cmNixCustomCommandGenerator nixGen(&cmd, lg, "Release", nullptr, nullptr);
    info.DerivationName = nixGen.GetDerivationName();
    
    customCommands[primaryOutput] = std::move(info);
  }
  
  // Also check for source files with custom commands
  std::vector<cmSourceFile*> sources;
  target->GetSourceFiles(sources, "Release");
  
  for (auto* sf : sources) {
    cmCustomCommand const* cc = sf->GetCustomCommand();
    if (!cc) {
      continue;
    }
    
    auto ccg = std::make_unique<cmCustomCommandGenerator>(*cc, "Release", lg);
    std::vector<std::string> outputs = ccg->GetOutputs();
    
    if (outputs.empty()) {
      continue;
    }
    
    std::string primaryOutput = outputs[0];
    
    // Skip if already processed
    if (customCommands.find(primaryOutput) != customCommands.end()) {
      continue;
    }
    
    CustomCommandInfo info;
    info.Output = primaryOutput;
    info.TargetName = target->GetName();
    info.Generator = ccg.release();
    info.Target = target;
    info.LocalGen = lg;
    
    // Collect dependencies
    std::vector<std::string> depends = info.Generator->GetDepends();
    for (const auto& dep : depends) {
      info.Depends.insert(dep);
    }
    
    // Get the derivation name from the actual generator
    // This ensures consistency with what will be written to the Nix file
    const cmCustomCommand& cmd = info.Generator->GetCC();
    cmNixCustomCommandGenerator nixGen(&cmd, lg, "Release", nullptr, nullptr);
    info.DerivationName = nixGen.GetDerivationName();
    
    customCommands[primaryOutput] = std::move(info);
  }
}

void cmNixCustomCommandHandler::WriteCustomCommandDerivations(
  const std::unordered_map<std::string, CustomCommandInfo>& customCommands,
  const std::map<std::string, std::string>* customCommandOutputs,
  const std::map<std::string, std::string>* objectFileOutputs,
  std::ostream& fout,
  const std::string& projectSourceDir,
  const std::string& projectBinaryDir,
  bool debugOutput)
{
  if (customCommands.empty()) {
    return;
  }
  
  // Detect cycles before processing
  if (this->DetectCustomCommandCycles(customCommands)) {
    return;
  }
  
  // Write each custom command derivation in dependency order
  // For now, write them in any order (the original code had topological sorting)
  for (const auto& [output, info] : customCommands) {
    if (debugOutput) {
      std::cerr << "[NIX-DEBUG] Writing custom command derivation: " 
                << info.DerivationName << std::endl;
    }
    
    // Get the custom command from the generator
    const cmCustomCommand& cc = info.Generator->GetCC();
    
    // Create a generator for this specific command with the output maps
    cmNixCustomCommandGenerator ccg(&cc, info.LocalGen, "Release", 
                                  customCommandOutputs, objectFileOutputs);
    ccg.Generate(reinterpret_cast<cmGeneratedFileStream&>(fout));
  }
}

bool cmNixCustomCommandHandler::DetectCustomCommandCycles(
  const std::unordered_map<std::string, CustomCommandInfo>& customCommands)
{
  return this->HasCycle(customCommands);
}

bool cmNixCustomCommandHandler::HasCycle(
  const std::unordered_map<std::string, CustomCommandInfo>& commands)
{
  std::set<std::string> visited;
  std::set<std::string> recursionStack;
  
  for (const auto& pair : commands) {
    if (visited.find(pair.first) == visited.end()) {
      if (this->DFSCycleCheck(pair.first, commands, visited, recursionStack)) {
        return true;
      }
    }
  }
  
  return false;
}

bool cmNixCustomCommandHandler::DFSCycleCheck(
  const std::string& output,
  const std::unordered_map<std::string, CustomCommandInfo>& commands,
  std::set<std::string>& visited,
  std::set<std::string>& recursionStack)
{
  visited.insert(output);
  recursionStack.insert(output);
  
  auto it = commands.find(output);
  if (it != commands.end()) {
    for (const auto& dep : it->second.Depends) {
      // Only check dependencies that are outputs of other custom commands
      if (commands.find(dep) != commands.end()) {
        if (recursionStack.find(dep) != recursionStack.end()) {
          std::cerr << "CMake Error: Circular dependency detected in custom commands:\n";
          std::cerr << "  " << output << " depends on " << dep 
                   << " which creates a cycle\n";
          return true;
        }
        
        if (visited.find(dep) == visited.end()) {
          if (this->DFSCycleCheck(dep, commands, visited, recursionStack)) {
            return true;
          }
        }
      }
    }
  }
  
  recursionStack.erase(output);
  return false;
}