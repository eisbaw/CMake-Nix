#include "cmNixCacheManager.h"

#include "cmGeneratorTarget.h"

#include <iostream>

cmNixCacheManager::cmNixCacheManager() = default;

cmNixCacheManager::~cmNixCacheManager() = default;

std::string cmNixCacheManager::GetDerivationName(
  const std::string& targetName,
  const std::string& sourceFile,
  std::function<std::string()> computeFunc)
{
  // Create cache key
  std::string cacheKey = targetName + "|" + sourceFile;
  
  // Check cache first
  {
    std::lock_guard<std::mutex> lock(this->CacheMutex);
    auto it = this->DerivationNameCache.find(cacheKey);
    if (it != this->DerivationNameCache.end()) {
      return it->second;
    }
  }
  
  // Compute the value outside the lock
  std::string result = computeFunc();
  
  // Cache the result
  {
    std::lock_guard<std::mutex> lock(this->CacheMutex);
    this->DerivationNameCache[cacheKey] = result;
    this->EvictDerivationNamesIfNeeded();
  }
  
  return result;
}

std::vector<std::string> cmNixCacheManager::GetLibraryDependencies(
  cmGeneratorTarget* target,
  const std::string& config,
  std::function<std::vector<std::string>()> computeFunc)
{
  std::pair<cmGeneratorTarget*, std::string> cacheKey = {target, config};
  
  // Double-checked locking pattern to prevent race condition
  {
    std::lock_guard<std::mutex> lock(this->CacheMutex);
    auto it = this->LibraryDependencyCache.find(cacheKey);
    if (it != this->LibraryDependencyCache.end()) {
      return it->second;
    }
  }
  
  // Compute outside the lock
  std::vector<std::string> result = computeFunc();
  
  // Check again inside lock in case another thread computed it
  {
    std::lock_guard<std::mutex> lock(this->CacheMutex);
    // Check if another thread already inserted while we were computing
    auto it = this->LibraryDependencyCache.find(cacheKey);
    if (it != this->LibraryDependencyCache.end()) {
      // Another thread beat us, use their result
      return it->second;
    }
    // We're the first, insert our result
    this->LibraryDependencyCache[cacheKey] = result;
    this->EvictLibraryDependenciesIfNeeded();
  }
  
  return result;
}

std::vector<std::string> cmNixCacheManager::GetTransitiveDependencies(
  const std::string& sourcePath,
  std::function<std::vector<std::string>()> computeFunc)
{
  // Check cache first
  {
    std::lock_guard<std::mutex> lock(this->CacheMutex);
    auto it = this->TransitiveDependencyCache.find(sourcePath);
    if (it != this->TransitiveDependencyCache.end()) {
      return it->second;
    }
  }
  
  // Compute outside the lock
  std::vector<std::string> result = computeFunc();
  
  // Cache the result
  {
    std::lock_guard<std::mutex> lock(this->CacheMutex);
    this->TransitiveDependencyCache[sourcePath] = result;
    this->EvictTransitiveDependenciesIfNeeded();
  }
  
  return result;
}

bool cmNixCacheManager::IsDerivationNameUsed(const std::string& name) const
{
  std::lock_guard<std::mutex> lock(this->CacheMutex);
  return this->UsedDerivationNames.find(name) != this->UsedDerivationNames.end();
}

void cmNixCacheManager::MarkDerivationNameUsed(const std::string& name)
{
  std::lock_guard<std::mutex> lock(this->CacheMutex);
  this->UsedDerivationNames.insert(name);
  this->EvictUsedDerivationNamesIfNeeded();
}

std::vector<std::string> cmNixCacheManager::GetSystemPaths(
  std::function<std::vector<std::string>()> computeFunc)
{
  // Check cache first
  {
    std::lock_guard<std::mutex> lock(this->CacheMutex);
    if (this->SystemPathsCached) {
      return this->SystemPathsCache;
    }
  }
  
  // Compute outside the lock
  std::vector<std::string> result = computeFunc();
  
  // Cache the result
  {
    std::lock_guard<std::mutex> lock(this->CacheMutex);
    this->SystemPathsCache = result;
    this->SystemPathsCached = true;
  }
  
  return result;
}

void cmNixCacheManager::ClearAll()
{
  std::lock_guard<std::mutex> lock(this->CacheMutex);
  this->DerivationNameCache.clear();
  this->LibraryDependencyCache.clear();
  this->TransitiveDependencyCache.clear();
  this->UsedDerivationNames.clear();
  this->CompilerInfoCache.clear();
  this->SystemPathsCache.clear();
  this->SystemPathsCached = false;
}

void cmNixCacheManager::ClearDerivationNames()
{
  std::lock_guard<std::mutex> lock(this->CacheMutex);
  this->DerivationNameCache.clear();
}

void cmNixCacheManager::ClearLibraryDependencies()
{
  std::lock_guard<std::mutex> lock(this->CacheMutex);
  this->LibraryDependencyCache.clear();
}

void cmNixCacheManager::ClearTransitiveDependencies()
{
  std::lock_guard<std::mutex> lock(this->CacheMutex);
  this->TransitiveDependencyCache.clear();
}

void cmNixCacheManager::ClearUsedDerivationNames()
{
  std::lock_guard<std::mutex> lock(this->CacheMutex);
  this->UsedDerivationNames.clear();
}

void cmNixCacheManager::ClearCompilerInfo()
{
  std::lock_guard<std::mutex> lock(this->CacheMutex);
  this->CompilerInfoCache.clear();
}

void cmNixCacheManager::ClearSystemPaths()
{
  std::lock_guard<std::mutex> lock(this->CacheMutex);
  this->SystemPathsCache.clear();
  this->SystemPathsCached = false;
}

cmNixCacheManager::CacheStats cmNixCacheManager::GetStats() const
{
  std::lock_guard<std::mutex> lock(this->CacheMutex);
  
  CacheStats stats;
  stats.DerivationNameCacheSize = this->DerivationNameCache.size();
  stats.LibraryDependencyCacheSize = this->LibraryDependencyCache.size();
  stats.TransitiveDependencyCacheSize = this->TransitiveDependencyCache.size();
  stats.UsedDerivationNamesSize = this->UsedDerivationNames.size();
  stats.CompilerInfoCacheSize = this->CompilerInfoCache.size();
  stats.SystemPathsCacheSize = this->SystemPathsCached ? 1 : 0;
  
  // Rough memory estimate
  size_t memoryEstimate = 0;
  
  // Derivation name cache: estimate 100 bytes per entry (strings)
  memoryEstimate += stats.DerivationNameCacheSize * 100;
  
  // Library dependency cache: estimate 500 bytes per entry (vector of strings)
  memoryEstimate += stats.LibraryDependencyCacheSize * 500;
  
  // Transitive dependency cache: estimate 200 bytes per entry (file path lists)
  memoryEstimate += stats.TransitiveDependencyCacheSize * 200;
  
  // Used derivation names: estimate 50 bytes per entry (name strings)
  memoryEstimate += stats.UsedDerivationNamesSize * 50;
  
  // Compiler info cache: estimate 200 bytes per entry (compiler structs)
  memoryEstimate += stats.CompilerInfoCacheSize * 200;
  
  // System paths cache: estimate 2KB for typical system path list
  memoryEstimate += stats.SystemPathsCacheSize * 2048;
  
  stats.TotalMemoryEstimate = memoryEstimate;
  
  return stats;
}

void cmNixCacheManager::EvictDerivationNamesIfNeeded()
{
  // Simple eviction: clear entire cache if it gets too large
  // A more sophisticated approach would use LRU eviction
  if (this->DerivationNameCache.size() > MAX_DERIVATION_NAME_CACHE_SIZE) {
    // Clear half the cache (simple strategy)
    auto it = this->DerivationNameCache.begin();
    size_t toRemove = this->DerivationNameCache.size() / 2;
    while (toRemove > 0 && it != this->DerivationNameCache.end()) {
      it = this->DerivationNameCache.erase(it);
      --toRemove;
    }
  }
}

void cmNixCacheManager::EvictLibraryDependenciesIfNeeded()
{
  // Simple eviction: clear entire cache if it gets too large
  if (this->LibraryDependencyCache.size() > MAX_LIBRARY_DEPENDENCY_CACHE_SIZE) {
    // Clear half the cache
    auto it = this->LibraryDependencyCache.begin();
    size_t toRemove = this->LibraryDependencyCache.size() / 2;
    while (toRemove > 0 && it != this->LibraryDependencyCache.end()) {
      it = this->LibraryDependencyCache.erase(it);
      --toRemove;
    }
  }
}

void cmNixCacheManager::EvictTransitiveDependenciesIfNeeded()
{
  // Simple eviction: clear entire cache if it gets too large
  if (this->TransitiveDependencyCache.size() > MAX_TRANSITIVE_DEPENDENCY_CACHE_SIZE) {
    // Clear half the cache
    auto it = this->TransitiveDependencyCache.begin();
    size_t toRemove = this->TransitiveDependencyCache.size() / 2;
    while (toRemove > 0 && it != this->TransitiveDependencyCache.end()) {
      it = this->TransitiveDependencyCache.erase(it);
      --toRemove;
    }
  }
}

void cmNixCacheManager::EvictUsedDerivationNamesIfNeeded()
{
  // Simple eviction: clear oldest entries if it gets too large
  if (this->UsedDerivationNames.size() > MAX_USED_DERIVATION_NAMES_SIZE) {
    // For a set, we can't easily remove "oldest", so we clear half
    auto it = this->UsedDerivationNames.begin();
    size_t toRemove = this->UsedDerivationNames.size() / 2;
    while (toRemove > 0 && it != this->UsedDerivationNames.end()) {
      it = this->UsedDerivationNames.erase(it);
      --toRemove;
    }
  }
}