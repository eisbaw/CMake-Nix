/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "cmNixCompilerResolver.h"
#include "cmNixBuildConfiguration.h"
#include "cmNixFileSystemHelper.h"
#include "cmGeneratorTarget.h"
#include "cmGlobalGenerator.h"
#include "cmLocalGenerator.h"
#include "cmMakefile.h"
#include "cmSourceFile.h"
#include "cmState.h"
#include "cmTarget.h"
#include "cmake.h"

#include "testCommon.h"

// Test fixture for refactored components
class NixComponentTestFixture
{
public:
  NixComponentTestFixture()
  {
    this->CMake = cm::make_unique<cmake>(cmake::RoleInternal, cmState::Unknown);
    this->CMake->SetHomeDirectory("/tmp/test_source");
    this->CMake->SetHomeOutputDirectory("/tmp/test_build");
  }

  std::unique_ptr<cmake> CMake;
};

// Test cmNixCompilerResolver
static bool testCompilerResolver()
{
  std::cout << "Testing cmNixCompilerResolver..." << std::endl;
  
  NixComponentTestFixture fixture;
  cmNixCompilerResolver resolver(fixture.CMake.get());
  
  // Test C compiler package detection
  std::string cPackage = resolver.GetCompilerPackage("C");
  if (cPackage != "gcc" && cPackage != "clang") {
    std::cerr << "FAIL: Expected gcc or clang for C compiler, got: " << cPackage << std::endl;
    return false;
  }
  std::cout << "  C compiler package: " << cPackage << std::endl;
  
  // Test C++ compiler package detection
  std::string cxxPackage = resolver.GetCompilerPackage("CXX");
  if (cxxPackage != "gcc" && cxxPackage != "clang") {
    std::cerr << "FAIL: Expected gcc or clang for CXX compiler, got: " << cxxPackage << std::endl;
    return false;
  }
  std::cout << "  C++ compiler package: " << cxxPackage << std::endl;
  
  // Test Fortran compiler package detection
  std::string fortranPackage = resolver.GetCompilerPackage("Fortran");
  if (fortranPackage != "gfortran") {
    std::cerr << "FAIL: Expected gfortran for Fortran compiler, got: " << fortranPackage << std::endl;
    return false;
  }
  std::cout << "  Fortran compiler package: " << fortranPackage << std::endl;
  
  // Test compiler commands
  std::string cCommand = resolver.GetCompilerCommand("C");
  if (cCommand != "gcc" && cCommand != "clang") {
    std::cerr << "FAIL: Expected gcc or clang command for C, got: " << cCommand << std::endl;
    return false;
  }
  std::cout << "  C compiler command: " << cCommand << std::endl;
  
  std::string cxxCommand = resolver.GetCompilerCommand("CXX");
  if (cxxCommand != "g++" && cxxCommand != "clang++") {
    std::cerr << "FAIL: Expected g++ or clang++ command for CXX, got: " << cxxCommand << std::endl;
    return false;
  }
  std::cout << "  C++ compiler command: " << cxxCommand << std::endl;
  
  // Test cache clearing
  resolver.ClearCache();
  std::cout << "  Cache cleared successfully" << std::endl;
  
  std::cout << "PASS: cmNixCompilerResolver tests" << std::endl;
  return true;
}

// Test cmNixBuildConfiguration
static bool testBuildConfiguration()
{
  std::cout << "\nTesting cmNixBuildConfiguration..." << std::endl;
  
  // Test default configuration
  std::string defaultConfig = cmNixBuildConfiguration::GetDefaultConfiguration();
  if (defaultConfig != "Release") {
    std::cerr << "FAIL: Expected Release as default config, got: " << defaultConfig << std::endl;
    return false;
  }
  std::cout << "  Default configuration: " << defaultConfig << std::endl;
  
  // Test configuration flags
  std::string debugFlags = cmNixBuildConfiguration::GetConfigurationFlags("Debug");
  if (debugFlags != "-g -O0") {
    std::cerr << "FAIL: Expected '-g -O0' for Debug, got: " << debugFlags << std::endl;
    return false;
  }
  std::cout << "  Debug flags: " << debugFlags << std::endl;
  
  std::string releaseFlags = cmNixBuildConfiguration::GetConfigurationFlags("Release");
  if (releaseFlags != "-O3 -DNDEBUG") {
    std::cerr << "FAIL: Expected '-O3 -DNDEBUG' for Release, got: " << releaseFlags << std::endl;
    return false;
  }
  std::cout << "  Release flags: " << releaseFlags << std::endl;
  
  // Test optimization detection
  if (!cmNixBuildConfiguration::IsOptimizedConfiguration("Release")) {
    std::cerr << "FAIL: Release should be optimized" << std::endl;
    return false;
  }
  if (cmNixBuildConfiguration::IsOptimizedConfiguration("Debug")) {
    std::cerr << "FAIL: Debug should not be optimized" << std::endl;
    return false;
  }
  std::cout << "  Optimization detection works correctly" << std::endl;
  
  // Test debug info detection
  if (!cmNixBuildConfiguration::HasDebugInfo("Debug")) {
    std::cerr << "FAIL: Debug should have debug info" << std::endl;
    return false;
  }
  if (!cmNixBuildConfiguration::HasDebugInfo("RelWithDebInfo")) {
    std::cerr << "FAIL: RelWithDebInfo should have debug info" << std::endl;
    return false;
  }
  if (cmNixBuildConfiguration::HasDebugInfo("Release")) {
    std::cerr << "FAIL: Release should not have debug info" << std::endl;
    return false;
  }
  std::cout << "  Debug info detection works correctly" << std::endl;
  
  std::cout << "PASS: cmNixBuildConfiguration tests" << std::endl;
  return true;
}

// Test cmNixFileSystemHelper
static bool testFileSystemHelper()
{
  std::cout << "\nTesting cmNixFileSystemHelper..." << std::endl;
  
  NixComponentTestFixture fixture;
  cmNixFileSystemHelper fsHelper(fixture.CMake.get());
  
  // Test system path detection
  if (!fsHelper.IsSystemPath("/usr/include")) {
    std::cerr << "FAIL: /usr/include should be a system path" << std::endl;
    return false;
  }
  if (!fsHelper.IsSystemPath("/nix/store/abc123-package")) {
    std::cerr << "FAIL: /nix/store paths should be system paths" << std::endl;
    return false;
  }
  if (fsHelper.IsSystemPath("/home/user/project")) {
    std::cerr << "FAIL: /home/user/project should not be a system path" << std::endl;
    return false;
  }
  std::cout << "  System path detection works correctly" << std::endl;
  
  // Test Nix store path detection
  if (!fsHelper.IsNixStorePath("/nix/store/abc123-package")) {
    std::cerr << "FAIL: /nix/store/abc123-package should be a Nix store path" << std::endl;
    return false;
  }
  if (fsHelper.IsNixStorePath("/usr/local/lib")) {
    std::cerr << "FAIL: /usr/local/lib should not be a Nix store path" << std::endl;
    return false;
  }
  std::cout << "  Nix store path detection works correctly" << std::endl;
  
  // Test external path detection
  std::string projectDir = "/home/user/project";
  std::string buildDir = "/home/user/project/build";
  
  if (!fsHelper.IsExternalPath("/usr/include/stdio.h", projectDir, buildDir)) {
    std::cerr << "FAIL: /usr/include/stdio.h should be external" << std::endl;
    return false;
  }
  if (fsHelper.IsExternalPath("/home/user/project/src/main.cpp", projectDir, buildDir)) {
    std::cerr << "FAIL: Project source file should not be external" << std::endl;
    return false;
  }
  if (fsHelper.IsExternalPath("/home/user/project/build/generated.h", projectDir, buildDir)) {
    std::cerr << "FAIL: Build directory file should not be external" << std::endl;
    return false;
  }
  std::cout << "  External path detection works correctly" << std::endl;
  
  // Test path security validation
  std::string errorMsg;
  if (!fsHelper.ValidatePathSecurity("/home/user/project/src/main.cpp", projectDir, buildDir, errorMsg)) {
    std::cerr << "FAIL: Valid project path should pass security check: " << errorMsg << std::endl;
    return false;
  }
  if (!errorMsg.empty()) {
    std::cerr << "FAIL: No error message expected for valid path" << std::endl;
    return false;
  }
  std::cout << "  Path security validation works correctly" << std::endl;
  
  // Test relative path computation
  std::string relPath = fsHelper.GetRelativePath("/home/user/project", "/home/user/project/src/main.cpp");
  if (relPath != "src/main.cpp") {
    std::cerr << "FAIL: Expected 'src/main.cpp', got: " << relPath << std::endl;
    return false;
  }
  std::cout << "  Relative path computation works correctly" << std::endl;
  
  // Test system path prefixes
  std::vector<std::string> sysPaths = fsHelper.GetSystemPathPrefixes();
  if (sysPaths.empty()) {
    std::cerr << "FAIL: System paths should not be empty" << std::endl;
    return false;
  }
  bool hasUsr = false;
  bool hasNixStore = false;
  for (const auto& path : sysPaths) {
    if (path == "/usr") hasUsr = true;
    if (path == "/nix/store") hasNixStore = true;
  }
  if (!hasUsr || !hasNixStore) {
    std::cerr << "FAIL: Expected /usr and /nix/store in system paths" << std::endl;
    return false;
  }
  std::cout << "  System path prefixes include expected paths" << std::endl;
  
  std::cout << "PASS: cmNixFileSystemHelper tests" << std::endl;
  return true;
}

// Test integration between components
static bool testComponentIntegration()
{
  std::cout << "\nTesting component integration..." << std::endl;
  
  NixComponentTestFixture fixture;
  
  // Create instances of all components
  cmNixCompilerResolver compilerResolver(fixture.CMake.get());
  cmNixFileSystemHelper fsHelper(fixture.CMake.get());
  
  // Test that components can work together
  std::string cPackage = compilerResolver.GetCompilerPackage("C");
  std::string cCommand = compilerResolver.GetCompilerCommand("C");
  
  std::cout << "  Compiler package for C: " << cPackage << std::endl;
  std::cout << "  Compiler command for C: " << cCommand << std::endl;
  
  // Test build configuration with null target
  std::string config = cmNixBuildConfiguration::GetBuildConfiguration(nullptr, nullptr);
  if (config != "Release") {
    std::cerr << "FAIL: Expected Release for null target, got: " << config << std::endl;
    return false;
  }
  std::cout << "  Build configuration for null target: " << config << std::endl;
  
  // Test path operations
  if (!fsHelper.IsSystemPath("/usr/bin/" + cCommand)) {
    std::cout << "  Note: Compiler " << cCommand << " not in /usr/bin (might be in Nix store)" << std::endl;
  }
  
  std::cout << "PASS: Component integration tests" << std::endl;
  return true;
}

// Main test runner
int testNixComponentRefactoring(int /*unused*/, char* /*unused*/[])
{
  bool allTestsPassed = true;
  
  allTestsPassed &= testCompilerResolver();
  allTestsPassed &= testBuildConfiguration();
  allTestsPassed &= testFileSystemHelper();
  allTestsPassed &= testComponentIntegration();
  
  if (allTestsPassed) {
    std::cout << "\nAll Nix component refactoring tests PASSED!" << std::endl;
    return 0;
  } else {
    std::cerr << "\nSome Nix component refactoring tests FAILED!" << std::endl;
    return 1;
  }
}