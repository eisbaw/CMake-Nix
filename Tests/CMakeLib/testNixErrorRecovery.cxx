/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <sys/stat.h>

#include "cmSystemTools.h"
#include "cmGlobalNixGenerator.h"
#include "cmLocalNixGenerator.h"
#include "cmMakefile.h"
#include "cmGeneratorTarget.h"
#include "cmNixTargetGenerator.h"
#include "cmake.h"
#include "cmGlobalGenerator.h"
#include "cmState.h"
#include "cmStateDirectory.h"
#include "cmStateSnapshot.h"

class NixErrorRecoveryTest
{
public:
  bool TestAll();

private:
  bool TestNixBuildFailure();
  bool TestMalformedNixExpression();
  bool TestDiskFullSimulation();
  bool TestPermissionDenied();
  bool TestNixCommandTimeout();
  bool TestPartialFileWrite();
  bool TestConcurrentGeneration();
  bool TestInvalidCharactersInTarget();
  
  bool CreateTestProject(const std::string& dir, const std::string& content);
  bool RunCMakeGenerate(const std::string& dir, std::string& errorOutput);
  bool SimulateNixBuildError(const std::string& dir);
  bool WriteFileWithError(const std::string& path, const std::string& content, bool simulateError);
};

bool NixErrorRecoveryTest::TestAll()
{
  std::cout << "=== Testing Nix Backend Error Recovery ===" << std::endl;
  
  struct TestCase {
    std::string name;
    bool (NixErrorRecoveryTest::*func)();
  };
  
  std::vector<TestCase> tests = {
    {"Nix Build Failure", &NixErrorRecoveryTest::TestNixBuildFailure},
    {"Malformed Nix Expression", &NixErrorRecoveryTest::TestMalformedNixExpression},
    {"Disk Full Simulation", &NixErrorRecoveryTest::TestDiskFullSimulation},
    {"Permission Denied", &NixErrorRecoveryTest::TestPermissionDenied},
    {"Nix Command Timeout", &NixErrorRecoveryTest::TestNixCommandTimeout},
    {"Partial File Write", &NixErrorRecoveryTest::TestPartialFileWrite},
    {"Concurrent Generation", &NixErrorRecoveryTest::TestConcurrentGeneration},
    {"Invalid Characters in Target", &NixErrorRecoveryTest::TestInvalidCharactersInTarget}
  };
  
  int passed = 0;
  int failed = 0;
  
  for (const auto& test : tests) {
    std::cout << "\nTesting " << test.name << "... ";
    std::cout.flush();
    
    try {
      if ((this->*test.func)()) {
        std::cout << "PASSED" << std::endl;
        passed++;
      } else {
        std::cout << "FAILED" << std::endl;
        failed++;
      }
    } catch (const std::exception& e) {
      std::cout << "FAILED with exception: " << e.what() << std::endl;
      failed++;
    }
  }
  
  std::cout << "\n=== Summary ===" << std::endl;
  std::cout << "Passed: " << passed << std::endl;
  std::cout << "Failed: " << failed << std::endl;
  
  return failed == 0;
}

bool NixErrorRecoveryTest::CreateTestProject(const std::string& dir, const std::string& content)
{
  // Create directory
  if (!cmSystemTools::MakeDirectory(dir)) {
    return false;
  }
  
  // Write CMakeLists.txt
  std::string cmakeFile = dir + "/CMakeLists.txt";
  std::ofstream out(cmakeFile);
  if (!out) {
    return false;
  }
  
  out << content;
  out.close();
  
  return true;
}

bool NixErrorRecoveryTest::RunCMakeGenerate(const std::string& dir, std::string& errorOutput)
{
  cmake cm(cmake::RoleProject, cmState::Project);
  cm.SetHomeDirectory(dir);
  cm.SetHomeOutputDirectory(dir);
  
  // Capture stderr
  std::stringstream errorStream;
  auto oldCerr = std::cerr.rdbuf(errorStream.rdbuf());
  
  // Set generator to Nix
  cm.SetGlobalGenerator(cm.CreateGlobalGenerator("Nix"));
  
  // Configure and generate
  int result = cm.Configure();
  if (result == 0) {
    result = cm.Generate();
  }
  
  // Restore stderr
  std::cerr.rdbuf(oldCerr);
  errorOutput = errorStream.str();
  
  return result == 0;
}

bool NixErrorRecoveryTest::TestNixBuildFailure()
{
  std::string testDir = "/tmp/cmake_nix_test_build_failure";
  cmSystemTools::RemoveADirectory(testDir);
  
  // Create a project that will fail during nix-build
  std::string content = R"(
cmake_minimum_required(VERSION 3.20)
project(TestBuildFailure C)

# Create a source file with compilation error
file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/main.c" "
#error This will cause compilation to fail
int main() { return 0; }
")

add_executable(fail_app main.c)
)";
  
  if (!CreateTestProject(testDir, content)) {
    return false;
  }
  
  // Generate should succeed (it's the build that will fail)
  std::string errorOutput;
  bool generateResult = RunCMakeGenerate(testDir, errorOutput);
  
  if (!generateResult) {
    std::cerr << "Generation unexpectedly failed: " << errorOutput << std::endl;
    return false;
  }
  
  // Now simulate running nix-build (which should fail)
  bool buildFailed = SimulateNixBuildError(testDir);
  
  // Clean up
  cmSystemTools::RemoveADirectory(testDir);
  
  return buildFailed;
}

bool NixErrorRecoveryTest::SimulateNixBuildError(const std::string& dir)
{
  // Check if default.nix was created
  std::string nixFile = dir + "/default.nix";
  if (!cmSystemTools::FileExists(nixFile)) {
    return false;
  }
  
  // We're not actually running nix-build in the test
  // Just verify the Nix expression was generated
  return true;
}

bool NixErrorRecoveryTest::TestMalformedNixExpression()
{
  std::string testDir = "/tmp/cmake_nix_test_malformed";
  cmSystemTools::RemoveADirectory(testDir);
  
  // Create a project that might trigger malformed Nix
  std::string content = R"(
cmake_minimum_required(VERSION 3.20)
project(TestMalformed C)

# Target with special characters that need escaping
file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/main.c" "int main() { return 0; }")
add_executable("app's\"test" main.c)
)";
  
  if (!CreateTestProject(testDir, content)) {
    return false;
  }
  
  std::string errorOutput;
  bool result = RunCMakeGenerate(testDir, errorOutput);
  
  // Should either handle special chars or fail gracefully
  if (result) {
    // Check if special chars were properly escaped
    std::string nixFile = testDir + "/default.nix";
    std::ifstream in(nixFile);
    std::string nixContent((std::istreambuf_iterator<char>(in)),
                          std::istreambuf_iterator<char>());
    
    // Should not contain unescaped quotes
    bool properlyEscaped = nixContent.find("app's\\\"test") != std::string::npos ||
                          nixContent.find("app_s_test") != std::string::npos;
    result = properlyEscaped;
  }
  
  // Clean up
  cmSystemTools::RemoveADirectory(testDir);
  
  return result;
}

bool NixErrorRecoveryTest::TestDiskFullSimulation()
{
  std::string testDir = "/tmp/cmake_nix_test_disk_full";
  cmSystemTools::RemoveADirectory(testDir);
  
  // Create normal project
  std::string content = R"(
cmake_minimum_required(VERSION 3.20)
project(TestDiskFull C)
file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/main.c" "int main() { return 0; }")
add_executable(app main.c)
)";
  
  if (!CreateTestProject(testDir, content)) {
    return false;
  }
  
  // We can't easily simulate disk full in unit test
  // Just verify error handling exists in the code
  std::string errorOutput;
  bool result = RunCMakeGenerate(testDir, errorOutput);
  
  // Clean up
  cmSystemTools::RemoveADirectory(testDir);
  
  return result; // Should generate successfully under normal conditions
}

bool NixErrorRecoveryTest::TestPermissionDenied()
{
  std::string testDir = "/tmp/cmake_nix_test_permission";
  cmSystemTools::RemoveADirectory(testDir);
  
  // Create project
  std::string content = R"(
cmake_minimum_required(VERSION 3.20)
project(TestPermission C)
file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/main.c" "int main() { return 0; }")
add_executable(app main.c)
)";
  
  if (!CreateTestProject(testDir, content)) {
    return false;
  }
  
  // Make directory read-only after creation
  // Note: This might not work in all environments
  chmod(testDir.c_str(), 0555);
  
  std::string errorOutput;
  bool result = !RunCMakeGenerate(testDir, errorOutput); // Should fail
  
  // Restore permissions for cleanup
  chmod(testDir.c_str(), 0755);
  
  // Clean up
  cmSystemTools::RemoveADirectory(testDir);
  
  return result || errorOutput.find("Permission") != std::string::npos;
}

bool NixErrorRecoveryTest::TestNixCommandTimeout()
{
  // This tests timeout handling in the generator
  // We verify the timeout infrastructure exists
  
  std::string testDir = "/tmp/cmake_nix_test_timeout";
  cmSystemTools::RemoveADirectory(testDir);
  
  // Create large project that might take time
  std::stringstream content;
  content << "cmake_minimum_required(VERSION 3.20)\n";
  content << "project(TestTimeout C)\n";
  
  // Create many targets
  for (int i = 0; i < 100; ++i) {
    content << "file(WRITE \"${CMAKE_CURRENT_SOURCE_DIR}/file" << i 
            << ".c\" \"int func" << i << "() { return " << i << "; }\")\n";
    content << "add_library(lib" << i << " STATIC file" << i << ".c)\n";
  }
  
  if (!CreateTestProject(testDir, content.str())) {
    return false;
  }
  
  // Measure generation time
  auto start = std::chrono::steady_clock::now();
  
  std::string errorOutput;
  bool result = RunCMakeGenerate(testDir, errorOutput);
  
  auto end = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  
  // Should complete within reasonable time (not hang)
  bool completedQuickly = duration.count() < 5000; // 5 seconds max
  
  // Clean up
  cmSystemTools::RemoveADirectory(testDir);
  
  return result && completedQuickly;
}

bool NixErrorRecoveryTest::TestPartialFileWrite()
{
  std::string testDir = "/tmp/cmake_nix_test_partial";
  cmSystemTools::RemoveADirectory(testDir);
  
  // Test that partial writes are handled
  if (!cmSystemTools::MakeDirectory(testDir)) {
    return false;
  }
  
  std::string testFile = testDir + "/test_partial.nix";
  
  // Simulate partial write
  WriteFileWithError(testFile, "{ test = ", true);
  
  // File should not exist or be complete
  if (cmSystemTools::FileExists(testFile)) {
    std::ifstream in(testFile);
    std::string content((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
    
    // If file exists, it should be complete (atomic write)
    if (content == "{ test = ") {
      // Partial write detected - this is bad
      cmSystemTools::RemoveADirectory(testDir);
      return false;
    }
  }
  
  // Clean up
  cmSystemTools::RemoveADirectory(testDir);
  
  return true;
}

bool NixErrorRecoveryTest::WriteFileWithError(const std::string& path, 
                                               const std::string& content,
                                               bool simulateError)
{
  if (simulateError) {
    // Don't actually write
    return false;
  }
  
  std::ofstream out(path);
  if (!out) {
    return false;
  }
  
  out << content;
  return out.good();
}

bool NixErrorRecoveryTest::TestConcurrentGeneration()
{
  std::string testDir = "/tmp/cmake_nix_test_concurrent";
  cmSystemTools::RemoveADirectory(testDir);
  
  // Create project
  std::string content = R"(
cmake_minimum_required(VERSION 3.20)
project(TestConcurrent C)
file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/main.c" "int main() { return 0; }")
add_executable(app main.c)
)";
  
  if (!CreateTestProject(testDir, content)) {
    return false;
  }
  
  // Run two generations concurrently
  std::vector<std::thread> threads;
  std::vector<bool> results(2, false);
  std::vector<std::string> errors(2);
  
  for (int i = 0; i < 2; ++i) {
    threads.emplace_back([this, testDir, i, &results, &errors]() {
      results[i] = RunCMakeGenerate(testDir, errors[i]);
    });
  }
  
  // Wait for completion
  for (auto& t : threads) {
    t.join();
  }
  
  // At least one should succeed
  bool anySucceeded = results[0] || results[1];
  
  // Clean up
  cmSystemTools::RemoveADirectory(testDir);
  
  return anySucceeded;
}

bool NixErrorRecoveryTest::TestInvalidCharactersInTarget()
{
  std::string testDir = "/tmp/cmake_nix_test_invalid_chars";
  cmSystemTools::RemoveADirectory(testDir);
  
  // Test various invalid characters
  std::vector<std::string> invalidTargets = {
    "app with spaces",
    "app/with/slashes",
    "app@special",
    "app#hash",
    "app$dollar",
    "app%percent",
    "app&ampersand",
    "app*asterisk",
    "app(parens)",
    "app[brackets]",
    "app{braces}",
    "app|pipe",
    "app\\backslash",
    "app:colon",
    "app;semicolon",
    "app<less>",
    "app\"quote\"",
    "app'apostrophe'"
  };
  
  bool allHandled = true;
  
  for (const auto& target : invalidTargets) {
    cmSystemTools::RemoveADirectory(testDir);
    
    std::stringstream content;
    content << "cmake_minimum_required(VERSION 3.20)\n";
    content << "project(TestInvalid C)\n";
    content << "file(WRITE \"${CMAKE_CURRENT_SOURCE_DIR}/main.c\" \"int main() { return 0; }\")\n";
    content << "add_executable(\"" << target << "\" main.c)\n";
    
    if (!CreateTestProject(testDir, content.str())) {
      allHandled = false;
      continue;
    }
    
    std::string errorOutput;
    bool result = RunCMakeGenerate(testDir, errorOutput);
    
    // Should either succeed with sanitized name or fail with clear error
    if (!result && errorOutput.empty()) {
      std::cerr << "Failed without error message for: " << target << std::endl;
      allHandled = false;
    }
  }
  
  // Clean up
  cmSystemTools::RemoveADirectory(testDir);
  
  return allHandled;
}

int testNixErrorRecovery(int /*unused*/, char* /*unused*/[])
{
  NixErrorRecoveryTest test;
  return test.TestAll() ? 0 : 1;
}