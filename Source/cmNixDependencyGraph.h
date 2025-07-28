/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */
#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <vector>

#include "cmStateTypes.h"

class cmGeneratorTarget;

class cmNixDependencyGraph
{
public:
  void AddTarget(const std::string& name, cmGeneratorTarget* target);
  void AddDependency(const std::string& from, const std::string& to);
  
  bool HasCircularDependency() const;
  
  std::vector<std::string> GetTopologicalOrder() const;
  std::vector<std::string> GetTopologicalOrderForLinking(const std::string& target) const;
  
  void Clear();
  
  const std::unordered_map<std::string, cmGeneratorTarget*>& GetTargets() const
  {
    return this->Targets;
  }
  
  const std::unordered_map<std::string, std::unordered_set<std::string>>& GetAdjacencyList() const
  {
    return this->AdjacencyList;
  }
  
  std::unordered_set<std::string> GetDependencies(const std::string& target) const;
  
  // Additional methods needed by cmGlobalNixGenerator
  std::set<std::string> GetTransitiveSharedLibraries(const std::string& target) const;
  std::set<std::string> GetAllTransitiveDependencies(const std::string& target) const;
  
private:
  struct DependencyNode {
    std::string targetName;
    cmStateEnums::TargetType type = cmStateEnums::UNKNOWN_LIBRARY;
    std::vector<std::string> directDependencies;
    mutable std::set<std::string> transitiveDependencies; // cached
    mutable bool transitiveDepsComputed = false;
  };
  
  bool DFSVisit(const std::string& node,
                std::unordered_map<std::string, int>& state,
                std::vector<std::string>& topologicalOrder) const;
  
  bool HasCycleFrom(const std::string& node,
                    std::unordered_set<std::string>& visited,
                    std::unordered_set<std::string>& recursionStack) const;
  
  void CollectTransitiveDependencies(const std::string& target,
                                     std::unordered_set<std::string>& visited,
                                     std::unordered_set<std::string>& dependencies) const;
  
  std::unordered_map<std::string, cmGeneratorTarget*> Targets;
  std::unordered_map<std::string, std::unordered_set<std::string>> AdjacencyList;
  std::unordered_map<std::string, DependencyNode> Nodes;
};