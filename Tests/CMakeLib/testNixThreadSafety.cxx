/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */

#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <mutex>
#include <set>

#include "cmNixCacheManager.h"
#include "cmNixCompilerResolver.h"
#include "cmGlobalNixGenerator.h"
#include "cmGeneratorTarget.h"
#include "cmake.h"
#include "cmState.h"

static bool testConcurrentCacheAccess()
{
  std::cout << "Testing concurrent cache access..." << std::endl;
  
  cmNixCacheManager cache;
  const int NUM_THREADS = 10;
  const int ITERATIONS_PER_THREAD = 100;
  
  std::atomic<int> computeCount{0};
  std::mutex resultMutex;
  std::set<std::string> allResults;
  
  // Test concurrent derivation name caching
  std::vector<std::thread> threads;
  for (int i = 0; i < NUM_THREADS; ++i) {
    threads.emplace_back([&cache, &computeCount, &resultMutex, &allResults, i]() {
      for (int j = 0; j < ITERATIONS_PER_THREAD; ++j) {
        std::string targetName = "target" + std::to_string(j % 5);
        std::string sourceFile = "source" + std::to_string(j % 3) + ".cpp";
        
        std::string result = cache.GetDerivationName(targetName, sourceFile, 
          [&computeCount, &targetName, &sourceFile]() {
            computeCount++;
            // Simulate some computation time
            std::this_thread::sleep_for(std::chrono::microseconds(10));
            return targetName + "_" + sourceFile + "_derivation";
          });
        
        {
          std::lock_guard<std::mutex> lock(resultMutex);
          allResults.insert(result);
        }
      }
    });
  }
  
  // Wait for all threads to complete
  for (auto& t : threads) {
    t.join();
  }
  
  // Verify results
  std::cout << "  Total computations: " << computeCount << std::endl;
  std::cout << "  Unique results: " << allResults.size() << std::endl;
  std::cout << "  Expected unique results: 15" << std::endl; // 5 targets * 3 sources
  
  // Each unique target/source combination should be computed only once (ideally)
  // Allow some extra computations due to race conditions
  // With 10 threads and potential race conditions, we might see more computations
  if (computeCount > 150) { // 15 unique combinations * 10 threads worst case
    std::cerr << "Too many computations! Cache may not be working properly." << std::endl;
    return false;
  }
  
  // A reasonable expectation is that we have fewer computations than total accesses
  int totalAccesses = NUM_THREADS * ITERATIONS_PER_THREAD;
  if (computeCount < totalAccesses / 10) {
    std::cout << "  Cache hit rate: " << (100.0 * (totalAccesses - computeCount) / totalAccesses) << "%" << std::endl;
  }
  
  if (allResults.size() != 15) {
    std::cerr << "Incorrect number of unique results!" << std::endl;
    return false;
  }
  
  // Verify cache stats
  auto stats = cache.GetStats();
  std::cout << "  Cache size: " << stats.DerivationNameCacheSize << std::endl;
  
  return true;
}

static bool testCacheEviction()
{
  std::cout << "Testing cache eviction..." << std::endl;
  
  cmNixCacheManager cache;
  
  // Fill cache beyond limit to trigger eviction
  const size_t ENTRIES_TO_ADD = 15000; // More than MAX_DERIVATION_NAME_CACHE_SIZE (10000)
  
  for (size_t i = 0; i < ENTRIES_TO_ADD; ++i) {
    std::string targetName = "target" + std::to_string(i);
    std::string sourceFile = "source" + std::to_string(i) + ".cpp";
    
    cache.GetDerivationName(targetName, sourceFile, [&targetName, &sourceFile]() {
      return targetName + "_" + sourceFile + "_derivation";
    });
  }
  
  auto stats = cache.GetStats();
  std::cout << "  Cache size after adding " << ENTRIES_TO_ADD << " entries: " 
            << stats.DerivationNameCacheSize << std::endl;
  
  // Cache should have been evicted to stay at or under limit
  // The eviction happens when size > MAX, so we might see exactly MAX
  if (stats.DerivationNameCacheSize > 10000) {
    std::cerr << "Cache eviction failed! Size: " << stats.DerivationNameCacheSize << std::endl;
    return false;
  }
  
  // Verify eviction happened (should be less than what we added)
  if (stats.DerivationNameCacheSize >= ENTRIES_TO_ADD) {
    std::cerr << "No eviction occurred! Added " << ENTRIES_TO_ADD 
              << " but cache has " << stats.DerivationNameCacheSize << std::endl;
    return false;
  }
  
  return true;
}

static bool testCompilerResolverThreadSafety()
{
  std::cout << "Testing compiler resolver thread safety..." << std::endl;
  
  // Create a cmake instance for the resolver
  cmake cm(cmake::RoleInternal, cmState::Unknown);
  cmNixCompilerResolver resolver(&cm);
  
  const int NUM_THREADS = 20;
  std::atomic<int> successCount{0};
  std::atomic<bool> hasError{false};
  
  std::vector<std::thread> threads;
  for (int i = 0; i < NUM_THREADS; ++i) {
    threads.emplace_back([&resolver, &successCount, &hasError]() {
      // Call GetCompilerPackage multiple times concurrently
      for (int j = 0; j < 100; ++j) {
        try {
          std::string pkg = resolver.GetCompilerPackage("C");
          if (!pkg.empty()) {
            successCount++;
          }
          
          pkg = resolver.GetCompilerPackage("CXX");
          if (!pkg.empty()) {
            successCount++;
          }
        } catch (const std::exception& e) {
          hasError = true;
          std::cerr << "Exception in thread: " << e.what() << std::endl;
        }
      }
    });
  }
  
  // Wait for all threads
  for (auto& t : threads) {
    t.join();
  }
  
  if (hasError) {
    std::cerr << "Errors occurred during concurrent access!" << std::endl;
    return false;
  }
  
  std::cout << "  Successfully completed " << successCount 
            << " concurrent compiler resolutions" << std::endl;
  
  return true;
}

static bool testConcurrentGeneratorAccess()
{
  std::cout << "Testing concurrent Nix generator access..." << std::endl;
  
  // Create a minimal cmake instance for testing
  cmake cm(cmake::RoleInternal, cmState::Unknown);
  auto gg = cm.CreateGlobalGenerator("Nix");
  if (!gg) {
    std::cerr << "Failed to create Nix generator!" << std::endl;
    return false;
  }
  
  auto* nixGen = static_cast<cmGlobalNixGenerator*>(gg.get());
  
  // Test concurrent access to various generator methods
  const int NUM_THREADS = 5;
  std::atomic<bool> hasError{false};
  
  std::vector<std::thread> threads;
  for (int i = 0; i < NUM_THREADS; ++i) {
    threads.emplace_back([nixGen, &hasError, i]() {
      try {
        // Access various thread-safe methods
        std::string name = nixGen->GetName();
        if (name != "Nix") {
          hasError = true;
          std::cerr << "Thread " << i << ": Wrong generator name!" << std::endl;
        }
        
        bool multiConfig = nixGen->IsMultiConfig();
        if (multiConfig) {
          hasError = true;
          std::cerr << "Thread " << i << ": Nix should not be multi-config!" << std::endl;
        }
        
        // Test other const methods
        nixGen->GetDocumentation();
        nixGen->GetAllTargetName();
        
      } catch (const std::exception& e) {
        hasError = true;
        std::cerr << "Thread " << i << " exception: " << e.what() << std::endl;
      }
    });
  }
  
  // Wait for all threads
  for (auto& t : threads) {
    t.join();
  }
  
  if (hasError) {
    return false;
  }
  
  std::cout << "  All concurrent accesses completed successfully" << std::endl;
  
  return true;
}

int testNixThreadSafety(int /*unused*/, char* /*unused*/[])
{
  std::cout << "=== Testing Nix Generator Thread Safety ===" << std::endl;
  
  int result = 0;
  
  if (!testConcurrentCacheAccess()) {
    std::cerr << "FAILED: Concurrent cache access test" << std::endl;
    result = 1;
  }
  
  if (!testCacheEviction()) {
    std::cerr << "FAILED: Cache eviction test" << std::endl;
    result = 1;
  }
  
  if (!testCompilerResolverThreadSafety()) {
    std::cerr << "FAILED: Compiler resolver thread safety test" << std::endl;
    result = 1;
  }
  
  if (!testConcurrentGeneratorAccess()) {
    std::cerr << "FAILED: Concurrent generator access test" << std::endl;
    result = 1;
  }
  
  if (result == 0) {
    std::cout << "\nAll thread safety tests PASSED!" << std::endl;
  }
  
  return result;
}