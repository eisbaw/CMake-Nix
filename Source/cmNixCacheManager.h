#pragma once

#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <utility>

class cmGeneratorTarget;

/**
 * \class cmNixCacheManager
 * \brief Manages all caching for the Nix generator
 *
 * This class consolidates all caching logic that was previously scattered
 * throughout cmGlobalNixGenerator. It provides thread-safe access to cached
 * values and implements proper cache invalidation strategies.
 */
class cmNixCacheManager
{
public:
  cmNixCacheManager();
  ~cmNixCacheManager();

  /**
   * Get or compute a derivation name for a target/source combination.
   * Thread-safe with proper locking.
   */
  std::string GetDerivationName(const std::string& targetName,
                               const std::string& sourceFile,
                               std::function<std::string()> computeFunc);

  /**
   * Get or compute library dependencies for a target/config combination.
   * Thread-safe with double-checked locking pattern.
   */
  std::vector<std::string> GetLibraryDependencies(
    cmGeneratorTarget* target,
    const std::string& config,
    std::function<std::vector<std::string>()> computeFunc);

  /**
   * Clear all caches. Should be called when configuration changes.
   */
  void ClearAll();

  /**
   * Clear derivation name cache only.
   */
  void ClearDerivationNames();

  /**
   * Clear library dependency cache only.
   */
  void ClearLibraryDependencies();

  /**
   * Get cache statistics for debugging/monitoring.
   */
  struct CacheStats {
    size_t DerivationNameCacheSize;
    size_t LibraryDependencyCacheSize;
    size_t TotalMemoryEstimate; // Rough estimate in bytes
  };
  CacheStats GetStats() const;

private:
  // Cache for derivation names: key is "targetName|sourceFile"
  mutable std::map<std::string, std::string> DerivationNameCache;
  
  // Cache for library dependencies: key is (target, config)
  mutable std::map<std::pair<cmGeneratorTarget*, std::string>, 
                  std::vector<std::string>> LibraryDependencyCache;
  
  // Single mutex protects all caches
  mutable std::mutex CacheMutex;

  // Maximum cache sizes (prevent unbounded growth)
  /**
   * Maximum number of derivation names to cache.
   * At 10,000 entries with ~100 bytes per entry (target|source string pairs),
   * this limits the cache to approximately 1 MB of memory.
   * This is sufficient for very large projects with thousands of source files.
   * 
   * To customize: Set CMAKE_NIX_DERIVATION_CACHE_SIZE environment variable.
   */
  static constexpr size_t MAX_DERIVATION_NAME_CACHE_SIZE = 10000;
  
  /**
   * Maximum number of library dependency results to cache.
   * At 1,000 entries with ~500 bytes per entry (vectors of library paths),
   * this limits the cache to approximately 500 KB of memory.
   * Most projects have far fewer than 1,000 unique target/config combinations.
   * 
   * To customize: Set CMAKE_NIX_LIBRARY_CACHE_SIZE environment variable.
   */
  static constexpr size_t MAX_LIBRARY_DEPENDENCY_CACHE_SIZE = 1000;

  // Evict oldest entries if cache grows too large
  void EvictDerivationNamesIfNeeded();
  void EvictLibraryDependenciesIfNeeded();
};