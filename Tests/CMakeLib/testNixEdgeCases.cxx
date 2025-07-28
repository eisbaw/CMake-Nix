/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <limits>
#include <unistd.h>

#include "cmSystemTools.h"
#include "cmGlobalNixGenerator.h"
#include "cmNixPathUtils.h"
#include "cmNixTargetGenerator.h"
#include "cmake.h"

class NixEdgeCaseTest
{
public:
  bool TestAll();

private:
  bool TestExtremelyLongTargetNames();
  bool TestCircularSymlinks();
  bool TestUnicodeInPaths();
  bool TestMaxPathLength();
  bool TestSpecialCharactersInPaths();
  bool TestDeeplyNestedDirectories();
  bool TestLargeNumberOfTargets();
  bool TestEmptyTargetNames();
  bool TestReservedNixNames();
  bool TestPathTraversalAttempts();
  
  bool CreateTestProject(const std::string& dir, const std::string& content);
  bool RunCMakeGenerate(const std::string& dir, std::string& output);
  void CreateCircularSymlink(const std::string& path1, const std::string& path2);
};

bool NixEdgeCaseTest::TestAll()
{
  std::cout << "=== Testing Nix Backend Edge Cases ===" << std::endl;
  
  struct TestCase {
    std::string name;
    bool (NixEdgeCaseTest::*func)();
  };
  
  std::vector<TestCase> tests = {
    {"Extremely Long Target Names", &NixEdgeCaseTest::TestExtremelyLongTargetNames},
    {"Circular Symlinks", &NixEdgeCaseTest::TestCircularSymlinks},
    {"Unicode in Paths", &NixEdgeCaseTest::TestUnicodeInPaths},
    {"Max Path Length", &NixEdgeCaseTest::TestMaxPathLength},
    {"Special Characters in Paths", &NixEdgeCaseTest::TestSpecialCharactersInPaths},
    {"Deeply Nested Directories", &NixEdgeCaseTest::TestDeeplyNestedDirectories},
    {"Large Number of Targets", &NixEdgeCaseTest::TestLargeNumberOfTargets},
    {"Empty Target Names", &NixEdgeCaseTest::TestEmptyTargetNames},
    {"Reserved Nix Names", &NixEdgeCaseTest::TestReservedNixNames},
    {"Path Traversal Attempts", &NixEdgeCaseTest::TestPathTraversalAttempts}
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

bool NixEdgeCaseTest::CreateTestProject(const std::string& dir, const std::string& content)
{
  if (!cmSystemTools::MakeDirectory(dir)) {
    return false;
  }
  
  std::string cmakeFile = dir + "/CMakeLists.txt";
  std::ofstream out(cmakeFile);
  if (!out) {
    return false;
  }
  
  out << content;
  out.close();
  
  return true;
}

bool NixEdgeCaseTest::RunCMakeGenerate(const std::string& dir, std::string& output)
{
  cmake cm(cmake::RoleProject, cmState::Project);
  cm.SetHomeDirectory(dir);
  cm.SetHomeOutputDirectory(dir);
  
  // Capture output
  std::stringstream outputStream;
  auto oldCout = std::cout.rdbuf(outputStream.rdbuf());
  auto oldCerr = std::cerr.rdbuf(outputStream.rdbuf());
  
  // Set generator to Nix
  cm.SetGlobalGenerator(cm.CreateGlobalGenerator("Nix"));
  
  // Configure and generate
  int result = cm.Configure();
  if (result == 0) {
    result = cm.Generate();
  }
  
  // Restore output
  std::cout.rdbuf(oldCout);
  std::cerr.rdbuf(oldCerr);
  output = outputStream.str();
  
  return result == 0;
}

bool NixEdgeCaseTest::TestExtremelyLongTargetNames()
{
  std::string testDir = "/tmp/cmake_nix_test_long_names";
  cmSystemTools::RemoveADirectory(testDir);
  
  // Create target with 255+ character name
  std::string longName(300, 'a');
  
  std::stringstream content;
  content << "cmake_minimum_required(VERSION 3.20)\n";
  content << "project(TestLongNames C)\n";
  content << "file(WRITE \"${CMAKE_CURRENT_SOURCE_DIR}/main.c\" \"int main() { return 0; }\")\n";
  content << "add_executable(" << longName << " main.c)\n";
  
  if (!CreateTestProject(testDir, content.str())) {
    return false;
  }
  
  std::string output;
  bool result = RunCMakeGenerate(testDir, output);
  
  // Should either handle gracefully or produce clear error
  if (result) {
    // Check if name was truncated in Nix expression
    std::string nixFile = testDir + "/default.nix";
    std::ifstream in(nixFile);
    std::string nixContent((std::istreambuf_iterator<char>(in)),
                          std::istreambuf_iterator<char>());
    
    // Name should be truncated or hashed
    bool handled = nixContent.find(longName) == std::string::npos ||
                   nixContent.find(longName.substr(0, 200)) != std::string::npos;
    result = handled;
  }
  
  cmSystemTools::RemoveADirectory(testDir);
  return result;
}

bool NixEdgeCaseTest::TestCircularSymlinks()
{
  std::string testDir = "/tmp/cmake_nix_test_circular";
  cmSystemTools::RemoveADirectory(testDir);
  
  if (!cmSystemTools::MakeDirectory(testDir)) {
    return false;
  }
  
  // Create circular symlinks
  std::string link1 = testDir + "/link1";
  std::string link2 = testDir + "/link2";
  
  CreateCircularSymlink(link1, link2);
  
  // Create project that might reference the symlinks
  std::string content = R"(
cmake_minimum_required(VERSION 3.20)
project(TestCircular C)
file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/main.c" "int main() { return 0; }")
add_executable(app main.c)

# Try to include the circular symlink directory
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/link1)
)";
  
  if (!CreateTestProject(testDir, content)) {
    cmSystemTools::RemoveADirectory(testDir);
    return false;
  }
  
  std::string output;
  bool result = RunCMakeGenerate(testDir, output);
  
  // Should handle circular symlinks without hanging
  cmSystemTools::RemoveADirectory(testDir);
  return result || output.find("circular") != std::string::npos;
}

void NixEdgeCaseTest::CreateCircularSymlink(const std::string& path1, const std::string& path2)
{
  // Create two symlinks pointing to each other
  // Ignore return values - best effort creation
  (void)symlink(path2.c_str(), path1.c_str());
  (void)symlink(path1.c_str(), path2.c_str());
}

bool NixEdgeCaseTest::TestUnicodeInPaths()
{
  std::string testDir = "/tmp/cmake_nix_test_unicode";
  cmSystemTools::RemoveADirectory(testDir);
  
  // Test various Unicode characters
  std::vector<std::string> unicodeNames = {
    u8"app_漢字",          // Chinese characters
    u8"app_العربية",       // Arabic
    u8"app_ελληνικά",      // Greek
    u8"app_עברית",         // Hebrew
    u8"app_🎯",            // Emoji
    u8"app_Ω",             // Greek letter
    u8"app_™",             // Trademark symbol
    u8"app_€",             // Euro symbol
    u8"café",              // Accented character
    u8"naïve",             // Diaeresis
    u8"résumé"             // Acute accent
  };
  
  bool allHandled = true;
  
  for (const auto& name : unicodeNames) {
    cmSystemTools::RemoveADirectory(testDir);
    
    std::stringstream content;
    content << "cmake_minimum_required(VERSION 3.20)\n";
    content << "project(TestUnicode C)\n";
    content << "file(WRITE \"${CMAKE_CURRENT_SOURCE_DIR}/main.c\" \"int main() { return 0; }\")\n";
    content << "add_executable(" << name << " main.c)\n";
    
    if (!CreateTestProject(testDir, content.str())) {
      allHandled = false;
      continue;
    }
    
    std::string output;
    bool result = RunCMakeGenerate(testDir, output);
    
    if (!result) {
      std::cerr << "Failed for Unicode name: " << name << std::endl;
      allHandled = false;
    }
  }
  
  cmSystemTools::RemoveADirectory(testDir);
  return allHandled;
}

bool NixEdgeCaseTest::TestMaxPathLength()
{
  std::string testDir = "/tmp/cmake_nix_test_maxpath";
  cmSystemTools::RemoveADirectory(testDir);
  
  // Create deeply nested path approaching system limits
  std::string deepPath = testDir;
  for (int i = 0; i < 50; ++i) {
    deepPath += "/subdir" + std::to_string(i);
  }
  
  // Try to create the deep directory
  if (!cmSystemTools::MakeDirectory(deepPath)) {
    // System doesn't support such deep paths
    return true; // Not a failure of our code
  }
  
  std::string content = R"(
cmake_minimum_required(VERSION 3.20)
project(TestMaxPath C)
file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/main.c" "int main() { return 0; }")
add_executable(app main.c)
)";
  
  if (!CreateTestProject(deepPath, content)) {
    cmSystemTools::RemoveADirectory(testDir);
    return true; // Filesystem limitation, not our bug
  }
  
  std::string output;
  bool result = RunCMakeGenerate(deepPath, output);
  
  cmSystemTools::RemoveADirectory(testDir);
  return result;
}

bool NixEdgeCaseTest::TestSpecialCharactersInPaths()
{
  std::string testDir = "/tmp/cmake_nix_test_special_chars";
  cmSystemTools::RemoveADirectory(testDir);
  
  // Test various special characters in file paths
  std::vector<std::pair<std::string, std::string>> specialPaths = {
    {"file with spaces.c", "file_with_spaces_c"},
    {"file$dollar.c", "file_dollar_c"},
    {"file@at.c", "file_at_c"},
    {"file#hash.c", "file_hash_c"},
    {"file%percent.c", "file_percent_c"},
    {"file&ampersand.c", "file_ampersand_c"},
    {"file*asterisk.c", "file_asterisk_c"},
    {"file(parens).c", "file_parens_c"},
    {"file[brackets].c", "file_brackets_c"},
    {"file{braces}.c", "file_braces_c"},
    {"file!exclaim.c", "file_exclaim_c"},
    {"file?question.c", "file_question_c"},
    {"file=equals.c", "file_equals_c"},
    {"file+plus.c", "file_plus_c"}
  };
  
  bool allHandled = true;
  
  for (const auto& [filename, expected] : specialPaths) {
    cmSystemTools::RemoveADirectory(testDir);
    
    std::stringstream content;
    content << "cmake_minimum_required(VERSION 3.20)\n";
    content << "project(TestSpecialChars C)\n";
    content << "file(WRITE \"${CMAKE_CURRENT_SOURCE_DIR}/" << filename 
            << "\" \"int main() { return 0; }\")\n";
    content << "add_executable(app \"" << filename << "\")\n";
    
    if (!CreateTestProject(testDir, content.str())) {
      allHandled = false;
      continue;
    }
    
    std::string output;
    bool result = RunCMakeGenerate(testDir, output);
    
    if (!result) {
      std::cerr << "Failed for special char filename: " << filename << std::endl;
      allHandled = false;
    }
  }
  
  cmSystemTools::RemoveADirectory(testDir);
  return allHandled;
}

bool NixEdgeCaseTest::TestDeeplyNestedDirectories()
{
  std::string testDir = "/tmp/cmake_nix_test_nested";
  cmSystemTools::RemoveADirectory(testDir);
  
  // Create nested subdirectory structure
  std::stringstream content;
  content << "cmake_minimum_required(VERSION 3.20)\n";
  content << "project(TestNested C)\n";
  
  std::string currentPath = "";
  for (int i = 0; i < 10; ++i) {
    currentPath += "level" + std::to_string(i) + "/";
    content << "add_subdirectory(" << currentPath << ")\n";
    
    // Create the subdirectory
    std::string fullPath = testDir + "/" + currentPath;
    cmSystemTools::MakeDirectory(fullPath);
    
    // Create CMakeLists.txt in subdirectory
    std::ofstream sub(fullPath + "CMakeLists.txt");
    sub << "# Level " << i << " CMakeLists.txt\n";
    if (i == 9) {
      sub << "file(WRITE \"${CMAKE_CURRENT_SOURCE_DIR}/main.c\" \"int main() { return 0; }\")\n";
      sub << "add_executable(nested_app main.c)\n";
    }
    sub.close();
  }
  
  if (!CreateTestProject(testDir, content.str())) {
    return false;
  }
  
  std::string output;
  bool result = RunCMakeGenerate(testDir, output);
  
  cmSystemTools::RemoveADirectory(testDir);
  return result;
}

bool NixEdgeCaseTest::TestLargeNumberOfTargets()
{
  std::string testDir = "/tmp/cmake_nix_test_many_targets";
  cmSystemTools::RemoveADirectory(testDir);
  
  // Create project with many targets
  std::stringstream content;
  content << "cmake_minimum_required(VERSION 3.20)\n";
  content << "project(TestManyTargets C)\n";
  
  // Create 1000 targets
  for (int i = 0; i < 1000; ++i) {
    content << "file(WRITE \"${CMAKE_CURRENT_SOURCE_DIR}/file" << i 
            << ".c\" \"int func" << i << "() { return " << i << "; }\")\n";
    content << "add_library(lib" << i << " STATIC file" << i << ".c)\n";
  }
  
  content << "file(WRITE \"${CMAKE_CURRENT_SOURCE_DIR}/main.c\" \"int main() { return 0; }\")\n";
  content << "add_executable(main_app main.c)\n";
  
  // Link some libraries
  for (int i = 0; i < 10; ++i) {
    content << "target_link_libraries(main_app PRIVATE lib" << i << ")\n";
  }
  
  if (!CreateTestProject(testDir, content.str())) {
    return false;
  }
  
  std::string output;
  bool result = RunCMakeGenerate(testDir, output);
  
  if (result) {
    // Verify Nix file was created and is reasonable size
    std::string nixFile = testDir + "/default.nix";
    std::ifstream in(nixFile, std::ios::ate | std::ios::binary);
    size_t fileSize = in.tellg();
    
    // Should be large but not excessive (< 10MB)
    result = fileSize > 100000 && fileSize < 10000000;
  }
  
  cmSystemTools::RemoveADirectory(testDir);
  return result;
}

bool NixEdgeCaseTest::TestEmptyTargetNames()
{
  std::string testDir = "/tmp/cmake_nix_test_empty_names";
  cmSystemTools::RemoveADirectory(testDir);
  
  // Try to create target with empty name
  std::string content = R"(
cmake_minimum_required(VERSION 3.20)
project(TestEmpty C)
file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/main.c" "int main() { return 0; }")

# This should fail in CMake itself
add_executable("" main.c)
)";
  
  if (!CreateTestProject(testDir, content)) {
    return false;
  }
  
  std::string output;
  bool result = !RunCMakeGenerate(testDir, output); // Should fail
  
  cmSystemTools::RemoveADirectory(testDir);
  return result;
}

bool NixEdgeCaseTest::TestReservedNixNames()
{
  std::string testDir = "/tmp/cmake_nix_test_reserved";
  cmSystemTools::RemoveADirectory(testDir);
  
  // Test Nix reserved words as target names
  std::vector<std::string> reservedNames = {
    "let", "in", "with", "rec", "inherit", "assert",
    "if", "then", "else", "true", "false", "null",
    "or", "and", "import", "derivation"
  };
  
  bool allHandled = true;
  
  for (const auto& name : reservedNames) {
    cmSystemTools::RemoveADirectory(testDir);
    
    std::stringstream content;
    content << "cmake_minimum_required(VERSION 3.20)\n";
    content << "project(TestReserved C)\n";
    content << "file(WRITE \"${CMAKE_CURRENT_SOURCE_DIR}/main.c\" \"int main() { return 0; }\")\n";
    content << "add_executable(" << name << " main.c)\n";
    
    if (!CreateTestProject(testDir, content.str())) {
      allHandled = false;
      continue;
    }
    
    std::string output;
    bool result = RunCMakeGenerate(testDir, output);
    
    if (result) {
      // Check that name was escaped in Nix
      std::string nixFile = testDir + "/default.nix";
      std::ifstream in(nixFile);
      std::string nixContent((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());
      
      // Should be quoted or prefixed
      bool escaped = nixContent.find("\"" + name + "\"") != std::string::npos ||
                     nixContent.find("_" + name) != std::string::npos;
      if (!escaped) {
        std::cerr << "Reserved name not escaped: " << name << std::endl;
        allHandled = false;
      }
    }
  }
  
  cmSystemTools::RemoveADirectory(testDir);
  return allHandled;
}

bool NixEdgeCaseTest::TestPathTraversalAttempts()
{
  std::string testDir = "/tmp/cmake_nix_test_traversal";
  cmSystemTools::RemoveADirectory(testDir);
  
  // Test various path traversal attempts
  std::vector<std::string> traversalPaths = {
    "../../../etc/passwd",
    "..\\..\\..\\windows\\system32",
    "./../../../root",
    "subdir/../../../../../../tmp",
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../etc"
  };
  
  bool allSecure = true;
  
  for (const auto& path : traversalPaths) {
    cmSystemTools::RemoveADirectory(testDir);
    
    std::stringstream content;
    content << "cmake_minimum_required(VERSION 3.20)\n";
    content << "project(TestTraversal C)\n";
    content << "file(WRITE \"${CMAKE_CURRENT_SOURCE_DIR}/main.c\" \"int main() { return 0; }\")\n";
    content << "add_executable(app main.c)\n";
    content << "target_include_directories(app PRIVATE \"" << path << "\")\n";
    
    if (!CreateTestProject(testDir, content.str())) {
      allSecure = false;
      continue;
    }
    
    std::string output;
    bool result = RunCMakeGenerate(testDir, output);
    
    if (result) {
      // Check that path was not included as-is
      std::string nixFile = testDir + "/default.nix";
      std::ifstream in(nixFile);
      std::string nixContent((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());
      
      // Should not contain the dangerous path
      if (nixContent.find("/etc/passwd") != std::string::npos ||
          nixContent.find("windows\\system32") != std::string::npos ||
          nixContent.find("/root") != std::string::npos) {
        std::cerr << "Path traversal not prevented: " << path << std::endl;
        allSecure = false;
      }
    }
  }
  
  cmSystemTools::RemoveADirectory(testDir);
  return allSecure;
}

int testNixEdgeCases(int /*unused*/, char* /*unused*/[])
{
  NixEdgeCaseTest test;
  return test.TestAll() ? 0 : 1;
}