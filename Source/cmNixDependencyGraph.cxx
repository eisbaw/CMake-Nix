/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */
#include "cmNixDependencyGraph.h"

#include <algorithm>
#include <functional>
#include <queue>
#include <stdexcept>

#include "cmGeneratorTarget.h"
#include "cmSystemTools.h"

void cmNixDependencyGraph::AddTarget(const std::string& name, cmGeneratorTarget* target)
{
  this->Targets[name] = target;
  if (this->AdjacencyList.find(name) == this->AdjacencyList.end()) {
    this->AdjacencyList[name] = std::unordered_set<std::string>();
  }
  
  // Initialize node with target type
  DependencyNode& node = this->Nodes[name];
  node.targetName = name;
  if (target) {
    node.type = target->GetType();
  }
}

void cmNixDependencyGraph::AddDependency(const std::string& from, const std::string& to)
{
  this->AdjacencyList[from].insert(to);
  
  if (this->AdjacencyList.find(to) == this->AdjacencyList.end()) {
    this->AdjacencyList[to] = std::unordered_set<std::string>();
  }
  
  // Update node's direct dependencies
  if (this->Nodes.find(from) != this->Nodes.end()) {
    this->Nodes[from].directDependencies.push_back(to);
  }
}

bool cmNixDependencyGraph::HasCircularDependency() const
{
  std::unordered_set<std::string> visited;
  std::unordered_set<std::string> recursionStack;
  
  for (const auto& pair : this->AdjacencyList) {
    if (visited.find(pair.first) == visited.end()) {
      if (this->HasCycleFrom(pair.first, visited, recursionStack)) {
        return true;
      }
    }
  }
  
  return false;
}

bool cmNixDependencyGraph::HasCircularDependency(const std::string& ignoreFlag) const
{
  if (!ignoreFlag.empty()) {
    const char* envValue = cmSystemTools::GetEnv(ignoreFlag);
    if (envValue && std::string(envValue) == "1") {
      return false;
    }
  }
  
  return this->HasCircularDependency();
}

void cmNixDependencyGraph::Clear()
{
  this->Targets.clear();
  this->AdjacencyList.clear();
  this->Nodes.clear();
}

std::vector<std::string> cmNixDependencyGraph::GetTopologicalOrder() const
{
  std::unordered_map<std::string, int> state;
  std::vector<std::string> topologicalOrder;
  
  for (const auto& pair : this->AdjacencyList) {
    state[pair.first] = 0;
  }
  
  for (const auto& pair : this->AdjacencyList) {
    if (state[pair.first] == 0) {
      if (!this->DFSVisit(pair.first, state, topologicalOrder)) {
        return {};
      }
    }
  }
  
  std::reverse(topologicalOrder.begin(), topologicalOrder.end());
  return topologicalOrder;
}

std::vector<std::string> cmNixDependencyGraph::GetTopologicalOrderForLinking(const std::string& target) const
{
  std::unordered_set<std::string> visited;
  std::unordered_set<std::string> relevant;
  
  this->CollectTransitiveDependencies(target, visited, relevant);
  relevant.insert(target);
  
  std::unordered_map<std::string, int> state;
  std::vector<std::string> topologicalOrder;
  
  for (const auto& t : relevant) {
    state[t] = 0;
  }
  
  std::function<bool(const std::string&, std::unordered_map<std::string, int>&, std::vector<std::string>&)> relevantDFS;
  relevantDFS = [this, &relevant, &relevantDFS](const std::string& node,
                                       std::unordered_map<std::string, int>& s,
                                       std::vector<std::string>& order) -> bool {
    s[node] = 1;
    
    auto it = this->AdjacencyList.find(node);
    if (it != this->AdjacencyList.end()) {
      for (const auto& neighbor : it->second) {
        if (relevant.find(neighbor) != relevant.end()) {
          if (s[neighbor] == 1) {
            return false;
          }
          if (s[neighbor] == 0) {
            if (!relevantDFS(neighbor, s, order)) {
              return false;
            }
          }
        }
      }
    }
    
    s[node] = 2;
    order.push_back(node);
    return true;
  };
  
  for (const auto& t : relevant) {
    if (state[t] == 0) {
      if (!relevantDFS(t, state, topologicalOrder)) {
        return {};
      }
    }
  }
  
  // Don't reverse for linking - we want dependencies to come after the targets that depend on them
  // std::reverse(topologicalOrder.begin(), topologicalOrder.end());
  return topologicalOrder;
}

std::unordered_set<std::string> cmNixDependencyGraph::GetDependencies(const std::string& target) const
{
  auto it = this->AdjacencyList.find(target);
  if (it != this->AdjacencyList.end()) {
    return it->second;
  }
  return {};
}

bool cmNixDependencyGraph::DFSVisit(const std::string& node,
                                    std::unordered_map<std::string, int>& state,
                                    std::vector<std::string>& topologicalOrder) const
{
  state[node] = 1;
  
  auto it = this->AdjacencyList.find(node);
  if (it != this->AdjacencyList.end()) {
    for (const auto& neighbor : it->second) {
      if (state[neighbor] == 1) {
        return false;
      }
      if (state[neighbor] == 0) {
        if (!this->DFSVisit(neighbor, state, topologicalOrder)) {
          return false;
        }
      }
    }
  }
  
  state[node] = 2;
  topologicalOrder.push_back(node);
  return true;
}

bool cmNixDependencyGraph::HasCycleFrom(const std::string& node,
                                        std::unordered_set<std::string>& visited,
                                        std::unordered_set<std::string>& recursionStack) const
{
  visited.insert(node);
  recursionStack.insert(node);
  
  auto it = this->AdjacencyList.find(node);
  if (it != this->AdjacencyList.end()) {
    for (const auto& neighbor : it->second) {
      if (visited.find(neighbor) == visited.end()) {
        if (this->HasCycleFrom(neighbor, visited, recursionStack)) {
          return true;
        }
      } else if (recursionStack.find(neighbor) != recursionStack.end()) {
        return true;
      }
    }
  }
  
  recursionStack.erase(node);
  return false;
}

void cmNixDependencyGraph::CollectTransitiveDependencies(const std::string& target,
                                                         std::unordered_set<std::string>& visited,
                                                         std::unordered_set<std::string>& dependencies) const
{
  if (visited.find(target) != visited.end()) {
    return;
  }
  
  visited.insert(target);
  
  auto it = this->AdjacencyList.find(target);
  if (it != this->AdjacencyList.end()) {
    for (const auto& dep : it->second) {
      dependencies.insert(dep);
      this->CollectTransitiveDependencies(dep, visited, dependencies);
    }
  }
}

std::set<std::string> cmNixDependencyGraph::GetTransitiveSharedLibraries(const std::string& target) const
{
  auto it = this->Nodes.find(target);
  if (it == this->Nodes.end()) {
    return {};
  }
  
  auto& node = it->second;
  
  // Return cached result if available
  if (node.transitiveDepsComputed) {
    return node.transitiveDependencies;
  }
  
  // Compute transitive dependencies using DFS
  std::set<std::string> visited;
  std::set<std::string> result;
  std::vector<std::string> stack;
  
  stack.push_back(target);
  
  while (!stack.empty()) {
    std::string current = stack.back();
    stack.pop_back();
    
    if (visited.count(current)) continue;
    visited.insert(current);
    
    auto currentIt = this->Nodes.find(current);
    if (currentIt == this->Nodes.end()) continue;
    
    auto& currentNode = currentIt->second;
    
    // If this is a shared or module library (and not the starting target), include it
    if (current != target && 
        (currentNode.type == cmStateEnums::SHARED_LIBRARY || 
         currentNode.type == cmStateEnums::MODULE_LIBRARY)) {
      result.insert(current);
    }
    
    // Add direct dependencies to stack
    for (const auto& dep : currentNode.directDependencies) {
      if (!visited.count(dep)) {
        stack.push_back(dep);
      }
    }
  }
  
  // Cache the result
  node.transitiveDependencies = result;
  node.transitiveDepsComputed = true;
  
  return result;
}

std::set<std::string> cmNixDependencyGraph::GetAllTransitiveDependencies(const std::string& target) const
{
  auto it = this->Nodes.find(target);
  if (it == this->Nodes.end()) {
    return {};
  }
  
  // Compute all transitive dependencies using DFS (no filtering by type)
  std::set<std::string> visited;
  std::set<std::string> result;
  std::vector<std::string> stack;
  
  stack.push_back(target);
  
  while (!stack.empty()) {
    std::string current = stack.back();
    stack.pop_back();
    
    if (visited.count(current)) continue;
    visited.insert(current);
    
    auto currentIt = this->Nodes.find(current);
    if (currentIt == this->Nodes.end()) continue;
    
    auto& currentNode = currentIt->second;
    
    // Include all dependencies except the starting target
    if (current != target) {
      result.insert(current);
    }
    
    // Add direct dependencies to stack
    for (const auto& dep : currentNode.directDependencies) {
      if (!visited.count(dep)) {
        stack.push_back(dep);
      }
    }
  }
  
  return result;
}