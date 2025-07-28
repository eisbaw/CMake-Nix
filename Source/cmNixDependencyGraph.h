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

/**
 * @class cmNixDependencyGraph
 * @brief Manages build target dependencies for the Nix backend generator
 * 
 * This class implements a directed acyclic graph (DAG) to represent build target
 * dependencies in CMake projects. It provides algorithms for:
 * - Topological sorting of targets for correct build order
 * - Circular dependency detection using depth-first search (DFS)
 * - Transitive dependency resolution for linking
 * 
 * ## Algorithm Details
 * 
 * ### Topological Sort Algorithm
 * The class uses a DFS-based topological sort algorithm with three-color marking:
 * - White (0): Unvisited node
 * - Gray (1): Currently being processed (in recursion stack)
 * - Black (2): Completely processed
 * 
 * Time Complexity: O(V + E) where V = vertices (targets), E = edges (dependencies)
 * Space Complexity: O(V) for the recursion stack and state tracking
 * 
 * ### Cycle Detection Algorithm
 * Uses DFS with a recursion stack to detect cycles:
 * - If we encounter a node already in the recursion stack, a cycle exists
 * - This ensures we only detect back edges, not cross edges
 * 
 * Time Complexity: O(V + E)
 * Space Complexity: O(V) for visited set and recursion stack
 * 
 * ### Transitive Dependency Resolution
 * Implements iterative DFS with caching for performance:
 * - Results are cached per target to avoid recomputation
 * - Separate methods for shared libraries vs all dependencies
 * - Uses iterative approach to avoid stack overflow on deep graphs
 * 
 * Time Complexity: O(V + E) for first computation, O(1) for cached results
 * Space Complexity: O(V) for the cache
 * 
 * ## Thread Safety
 * This class is NOT thread-safe. External synchronization is required if
 * accessed from multiple threads. The caching in GetTransitiveSharedLibraries
 * uses mutable members which require mutex protection in multi-threaded contexts.
 */
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