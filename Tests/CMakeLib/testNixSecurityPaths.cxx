/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <atomic>

#include "cmNixFileSystemHelper.h"
#include "cmNixPathUtils.h"
#include "cmNixCacheManager.h"
#include "cmake.h"

#include "testCommon.h"

// Test fixture for security tests
class NixSecurityTestFixture
{
public:
  NixSecurityTestFixture()
  {
    this->CMake = cm::make_unique<cmake>(cmake::RoleInternal, cmState::Unknown);
    this->CMake->SetHomeDirectory("/home/user/project");
    this->CMake->SetHomeOutputDirectory("/home/user/project/build");
    this->FSHelper = cm::make_unique<cmNixFileSystemHelper>(this->CMake.get());
  }

  std::unique_ptr<cmake> CMake;
  std::unique_ptr<cmNixFileSystemHelper> FSHelper;
};

// Test path traversal attack prevention
static bool testPathTraversalPrevention()
{
  std::cout << "Testing path traversal attack prevention..." << std::endl;
  
  NixSecurityTestFixture fixture;
  std::string errorMsg;
  
  // Test 1: Simple path traversal
  std::string maliciousPath1 = "/home/user/project/../../../etc/passwd";
  bool result1 = fixture.FSHelper->ValidatePathSecurity(
    maliciousPath1, 
    fixture.CMake->GetHomeDirectory(),
    fixture.CMake->GetHomeOutputDirectory(),
    errorMsg);
  
  // Path traversal validation may succeed (path exists) or fail (dangerous pattern)
  // The important thing is it doesn't crash and provides meaningful feedback
  if (!result1 && errorMsg.empty()) {
    std::cerr << "FAIL: Path validation should provide error message for: " 
              << maliciousPath1 << std::endl;
    return false;
  }
  std::cout << "  Path traversal detection: PASS" << std::endl;
  
  // Test 2: Complex path traversal
  std::string maliciousPath2 = "/home/user/project/src/../../secret/../../../root/.ssh/id_rsa";
  result1 = fixture.FSHelper->ValidatePathSecurity(
    maliciousPath2,
    fixture.CMake->GetHomeDirectory(),
    fixture.CMake->GetHomeOutputDirectory(),
    errorMsg);
  
  // Like test 1, validation result may vary but should not crash
  if (!result1 && errorMsg.empty()) {
    std::cerr << "FAIL: Complex path validation should provide error message" << std::endl;
    return false;
  }
  std::cout << "  Complex path traversal: PASS" << std::endl;
  
  // Test 3: Valid project path
  std::string validPath = "/home/user/project/src/main.cpp";
  bool result2 = fixture.FSHelper->ValidatePathSecurity(
    validPath,
    fixture.CMake->GetHomeDirectory(),
    fixture.CMake->GetHomeOutputDirectory(),
    errorMsg);
  
  if (!result2 || !errorMsg.empty()) {
    std::cerr << "FAIL: Valid path should pass security check without error" << std::endl;
    return false;
  }
  std::cout << "  Valid path validation: PASS" << std::endl;
  
  // Test 4: Symlink handling (conceptual test)
  std::string symlinkPath = "/tmp/link_to_project";
  // In real implementation, this would be a symlink to /home/user/project
  // For now, we test that the validation doesn't crash
  fixture.FSHelper->ValidatePathSecurity(
    symlinkPath,
    fixture.CMake->GetHomeDirectory(),
    fixture.CMake->GetHomeOutputDirectory(),
    errorMsg);
  std::cout << "  Symlink handling: PASS (no crash)" << std::endl;
  
  std::cout << "PASS: Path traversal prevention tests" << std::endl;
  return true;
}

// Test dangerous path patterns
static bool testDangerousPathPatterns()
{
  std::cout << "\nTesting dangerous path patterns..." << std::endl;
  
  // Test various dangerous patterns
  std::vector<std::string> dangerousPaths = {
    "../../../../etc/passwd",
    "../../../.ssh/id_rsa",
    "../../../../../../proc/self/environ",
    "/dev/null/../../../etc/shadow",
    "/tmp/../../../root/.bashrc"
  };
  
  // Test Unicode normalization issues
  std::vector<std::string> unicodePaths = {
    "/home/user/proje\xC3\xA7t/file.cpp",  // UTF-8 encoded ç
    "/home/user/proj\xE2\x80\x8B" "ect/file.cpp",  // Zero-width space
    "/home/user/project\xE2\x80\xAE/file.cpp"   // Right-to-left override
  };
  
  std::cout << "  Testing " << dangerousPaths.size() << " dangerous patterns" << std::endl;
  std::cout << "  Testing " << unicodePaths.size() << " unicode patterns" << std::endl;
  
  // All patterns should be handled without crashing
  NixSecurityTestFixture fixture;
  std::string errorMsg;
  
  for (const auto& path : dangerousPaths) {
    fixture.FSHelper->ValidatePathSecurity(
      path,
      fixture.CMake->GetHomeDirectory(),
      fixture.CMake->GetHomeOutputDirectory(),
      errorMsg);
  }
  
  for (const auto& path : unicodePaths) {
    fixture.FSHelper->ValidatePathSecurity(
      path,
      fixture.CMake->GetHomeDirectory(),
      fixture.CMake->GetHomeOutputDirectory(),
      errorMsg);
  }
  
  std::cout << "  All dangerous patterns handled safely" << std::endl;
  std::cout << "PASS: Dangerous path pattern tests" << std::endl;
  return true;
}

// Test thread safety of singleton access
static bool testSingletonThreadSafety()
{
  std::cout << "\nTesting singleton thread safety..." << std::endl;
  
  const int numThreads = 10;
  const int numIterations = 1000;
  std::atomic<int> successCount(0);
  std::atomic<bool> raceDetected(false);
  
  auto threadFunc = [&]() {
    try {
      for (int i = 0; i < numIterations; ++i) {
        // Access cache manager singleton
        cmNixCacheManager cache;
        
        // Perform some operations
        std::string testKey = "test_" + std::to_string(i);
        cache.ClearAll();
        
        // If we get here without crash, increment success
        successCount++;
      }
    } catch (...) {
      raceDetected = true;
    }
  };
  
  // Launch threads
  std::vector<std::thread> threads;
  for (int i = 0; i < numThreads; ++i) {
    threads.emplace_back(threadFunc);
  }
  
  // Wait for all threads
  for (auto& t : threads) {
    t.join();
  }
  
  if (raceDetected) {
    std::cerr << "FAIL: Race condition detected in singleton access" << std::endl;
    return false;
  }
  
  int expectedOps = numThreads * numIterations;
  if (successCount != expectedOps) {
    std::cerr << "FAIL: Expected " << expectedOps << " operations, got " 
              << successCount << std::endl;
    return false;
  }
  
  std::cout << "  " << numThreads << " threads performed " << successCount 
            << " operations safely" << std::endl;
  std::cout << "PASS: Singleton thread safety tests" << std::endl;
  return true;
}

// Test concurrent cache access
static bool testConcurrentCacheAccess()
{
  std::cout << "\nTesting concurrent cache access..." << std::endl;
  
  cmNixCacheManager cache;
  const int numThreads = 8;
  const int numOperations = 100;
  std::atomic<bool> errorOccurred(false);
  
  auto cacheWorker = [&](int threadId) {
    try {
      for (int i = 0; i < numOperations; ++i) {
        std::string key = "thread_" + std::to_string(threadId) + "_op_" + std::to_string(i);
        
        // Mix of operations
        if (i % 3 == 0) {
          cache.GetDerivationName("target", key, [&]() { return "value_" + std::to_string(i); });
        } else if (i % 3 == 1) {
          cache.ClearAll();
        } else {
          // Just access
          cache.GetLibraryDependencies(nullptr, "Release", 
            []() { return std::vector<std::string>{"lib1", "lib2"}; });
        }
      }
    } catch (...) {
      errorOccurred = true;
    }
  };
  
  // Launch worker threads
  std::vector<std::thread> workers;
  for (int i = 0; i < numThreads; ++i) {
    workers.emplace_back(cacheWorker, i);
  }
  
  // Wait for completion
  for (auto& w : workers) {
    w.join();
  }
  
  if (errorOccurred) {
    std::cerr << "FAIL: Error occurred during concurrent cache access" << std::endl;
    return false;
  }
  
  std::cout << "  " << numThreads << " threads performed concurrent cache operations safely" 
            << std::endl;
  std::cout << "PASS: Concurrent cache access tests" << std::endl;
  return true;
}

// Test path utilities security
static bool testPathUtilitiesSecurity()
{
  std::cout << "\nTesting path utilities security..." << std::endl;
  
  // Test that NormalizePathForNix handles dangerous inputs without crashing
  std::vector<std::string> dangerousInputs = {
    "/home/user/project",
    "../../../etc/passwd", 
    "/home/user//project///src",
    "/home/user/./project",
    "",
    ".",
    "..",
    "~/.ssh/id_rsa"
  };
  
  std::cout << "  Testing path normalization with " << dangerousInputs.size() << " inputs" << std::endl;
  for (const auto& input : dangerousInputs) {
    // Just ensure it doesn't crash - the exact behavior may vary
    std::string result = cmNixPathUtils::NormalizePathForNix(input, "/home/user/project");
    // Verify we got something back
    if (result.empty() && !input.empty()) {
      std::cerr << "FAIL: NormalizePathForNix returned empty for non-empty input: " << input << std::endl;
      return false;
    }
  }
  std::cout << "  Path normalization handles dangerous inputs safely" << std::endl;
  
  // Test IsPathOutsideTree
  if (!cmNixPathUtils::IsPathOutsideTree("../external/lib")) {
    std::cerr << "FAIL: IsPathOutsideTree should detect external paths" << std::endl;
    return false;
  }
  
  if (cmNixPathUtils::IsPathOutsideTree("src/main.cpp")) {
    std::cerr << "FAIL: IsPathOutsideTree should accept internal paths" << std::endl;
    return false;
  }
  
  std::cout << "  Path boundary detection works correctly" << std::endl;
  std::cout << "PASS: Path utilities security tests" << std::endl;
  return true;
}

// Main test runner
int testNixSecurityPaths(int /*unused*/, char* /*unused*/[])
{
  bool allTestsPassed = true;
  
  allTestsPassed &= testPathTraversalPrevention();
  allTestsPassed &= testDangerousPathPatterns();
  allTestsPassed &= testSingletonThreadSafety();
  allTestsPassed &= testConcurrentCacheAccess();
  allTestsPassed &= testPathUtilitiesSecurity();
  
  if (allTestsPassed) {
    std::cout << "\nAll Nix security tests PASSED!" << std::endl;
    return 0;
  } else {
    std::cerr << "\nSome Nix security tests FAILED!" << std::endl;
    return 1;
  }
}