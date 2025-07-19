cmake_minimum_required(VERSION 3.20)
project(FeatureDetectionTest CXX)

# Switch between basic and comprehensive test
option(COMPREHENSIVE_TEST "Run comprehensive feature detection test" OFF)

# This test project focuses on C++11 feature detection
# It uses features that require compiler support detection

# Test basic C++11 features that need detection
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Test features that require try-compile in traditional CMake
include(CheckCXXSourceCompiles)

# Test constexpr support (requires compilation test)
check_cxx_source_compiles("
constexpr int getNum() { return 42; }
int main() {
  constexpr int value = getNum();
  return value - 42;
}
" HAVE_CONSTEXPR)

# Test lambda support (requires compilation test) 
check_cxx_source_compiles("
void someFunc() {
  auto lambda = []() { return 42; };
  lambda();
}
int main() {
  someFunc();
  return 0;
}
" HAVE_LAMBDAS)

# Test auto type support (requires compilation test)
check_cxx_source_compiles("
int main() {
  auto value = 42;
  return value - 42;
}
" HAVE_AUTO_TYPE)

# Test nullptr support (requires compilation test)
check_cxx_source_compiles("
int main() {
  int* ptr = nullptr;
  return ptr == nullptr ? 0 : 1;
}
" HAVE_NULLPTR)

# Test range-based for loops (requires compilation test)
check_cxx_source_compiles("
#include <vector>
int main() {
  std::vector<int> vec = {1, 2, 3};
  for (auto& item : vec) {
    item++;
  }
  return 0;
}
" HAVE_RANGE_FOR)

# Test static_assert (requires compilation test)
check_cxx_source_compiles("
int main() {
  static_assert(sizeof(int) >= 4, \"int must be at least 4 bytes\");
  return 0;
}
" HAVE_STATIC_ASSERT)

# Test variadic templates (requires compilation test)
check_cxx_source_compiles("
template<typename... Args>
void variadic_function(Args... args) {}

int main() {
  variadic_function(1, 2.0, \"hello\");
  return 0;
}
" HAVE_VARIADIC_TEMPLATES)

# Use target_compile_features which requires feature detection
add_executable(feature_test main.cpp)

# These should trigger feature detection in the Nix generator
target_compile_features(feature_test PRIVATE 
  cxx_std_11
  cxx_constexpr
  cxx_lambdas
  cxx_auto_type
  cxx_nullptr
  cxx_range_for
  cxx_static_assert
  cxx_variadic_templates
)

# Add compile definitions based on detected features
if(HAVE_CONSTEXPR)
  target_compile_definitions(feature_test PRIVATE HAVE_CONSTEXPR)
endif()

if(HAVE_LAMBDAS)
  target_compile_definitions(feature_test PRIVATE HAVE_LAMBDAS)
endif()

if(HAVE_AUTO_TYPE)
  target_compile_definitions(feature_test PRIVATE HAVE_AUTO_TYPE)
endif()

if(HAVE_NULLPTR)
  target_compile_definitions(feature_test PRIVATE HAVE_NULLPTR)
endif()

if(HAVE_RANGE_FOR)
  target_compile_definitions(feature_test PRIVATE HAVE_RANGE_FOR)
endif()

if(HAVE_STATIC_ASSERT)
  target_compile_definitions(feature_test PRIVATE HAVE_STATIC_ASSERT)
endif()

if(HAVE_VARIADIC_TEMPLATES)
  target_compile_definitions(feature_test PRIVATE HAVE_VARIADIC_TEMPLATES)
endif()

# Display detected features for verification
message(STATUS "Feature Detection Results:")
message(STATUS "  HAVE_CONSTEXPR: ${HAVE_CONSTEXPR}")
message(STATUS "  HAVE_LAMBDAS: ${HAVE_LAMBDAS}")
message(STATUS "  HAVE_AUTO_TYPE: ${HAVE_AUTO_TYPE}")
message(STATUS "  HAVE_NULLPTR: ${HAVE_NULLPTR}")
message(STATUS "  HAVE_RANGE_FOR: ${HAVE_RANGE_FOR}")
message(STATUS "  HAVE_STATIC_ASSERT: ${HAVE_STATIC_ASSERT}")
message(STATUS "  HAVE_VARIADIC_TEMPLATES: ${HAVE_VARIADIC_TEMPLATES}")
message(STATUS "  CMAKE_CXX_COMPILE_FEATURES: ${CMAKE_CXX_COMPILE_FEATURES}")

# Comprehensive feature test (enabled with -DCOMPREHENSIVE_TEST=ON)
if(COMPREHENSIVE_TEST)
  message(STATUS "Running COMPREHENSIVE feature detection test")
  
  # Include comprehensive test configuration
  include(comprehensive_test.cmake)
else()
  message(STATUS "Running BASIC feature detection test (use -DCOMPREHENSIVE_TEST=ON for full test)")
endif()