cmake_minimum_required(VERSION 3.20)
project(ComprehensiveFeatureTest CXX)

# Set C++11 as the standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Display all detected features for analysis
message(STATUS "Comprehensive Feature Detection Analysis:")
message(STATUS "CMAKE_CXX_COMPILE_FEATURES: ${CMAKE_CXX_COMPILE_FEATURES}")

# Create comprehensive feature test
add_executable(comprehensive_test comprehensive_main.cpp)

# Test all major C++11 features using target_compile_features
set(CXX11_FEATURES
  cxx_std_11
  cxx_alias_templates
  cxx_alignas
  cxx_alignof
  cxx_attributes
  cxx_auto_type
  cxx_constexpr
  cxx_decltype
  cxx_defaulted_functions
  cxx_deleted_functions
  cxx_explicit_conversions
  cxx_final
  cxx_lambdas
  cxx_noexcept
  cxx_nullptr
  cxx_override
  cxx_range_for
  cxx_rvalue_references
  cxx_static_assert
  cxx_variadic_templates
)

# Try to enable each feature and add compile definitions
foreach(feature ${CXX11_FEATURES})
  # Check if feature is supported
  if(${feature} IN_LIST CMAKE_CXX_COMPILE_FEATURES)
    message(STATUS "  ✓ ${feature} supported")
    target_compile_features(comprehensive_test PRIVATE ${feature})
    string(TOUPPER ${feature} FEATURE_UPPER)
    target_compile_definitions(comprehensive_test PRIVATE "FEATURE_${feature}")
  else()
    message(STATUS "  ✗ ${feature} NOT supported")
  endif()
endforeach()

# Add general C++11 detection
if(cxx_std_11 IN_LIST CMAKE_CXX_COMPILE_FEATURES)
  target_compile_definitions(comprehensive_test PRIVATE CMAKE_CXX_STANDARD_11_DETECTED)
endif()

# Test individual compilation features with check_cxx_source_compiles
include(CheckCXXSourceCompiles)

# Test unique_ptr availability (common CMake self-hosting requirement)
check_cxx_source_compiles("
#include <memory>
int main() {
  auto ptr = std::unique_ptr<int>(new int(42));
  return 0;
}
" HAVE_STD_UNIQUE_PTR)

if(HAVE_STD_UNIQUE_PTR)
  target_compile_definitions(comprehensive_test PRIVATE HAVE_STD_UNIQUE_PTR)
  message(STATUS "  ✓ std::unique_ptr available")
else()
  message(STATUS "  ✗ std::unique_ptr NOT available")
endif()

# Test make_unique availability (C++14 feature)
check_cxx_source_compiles("
#include <memory>
int main() {
  auto ptr = std::make_unique<int>(42);
  return 0;
}
" HAVE_STD_MAKE_UNIQUE)

if(HAVE_STD_MAKE_UNIQUE)
  target_compile_definitions(comprehensive_test PRIVATE HAVE_STD_MAKE_UNIQUE)
  message(STATUS "  ✓ std::make_unique available")
else()
  message(STATUS "  ✗ std::make_unique NOT available")
endif()

# Test thread_local storage (important for many projects)
check_cxx_source_compiles("
thread_local int thread_var = 42;
int main() {
  return thread_var;
}
" HAVE_THREAD_LOCAL)

if(HAVE_THREAD_LOCAL)
  target_compile_definitions(comprehensive_test PRIVATE HAVE_THREAD_LOCAL)
  message(STATUS "  ✓ thread_local storage available")
else()
  message(STATUS "  ✗ thread_local storage NOT available")
endif()

# Test noexcept specifier
check_cxx_source_compiles("
void test_func() noexcept {}
int main() {
  test_func();
  return 0;
}
" HAVE_NOEXCEPT)

if(HAVE_NOEXCEPT)
  target_compile_definitions(comprehensive_test PRIVATE HAVE_NOEXCEPT)
  message(STATUS "  ✓ noexcept specifier available")
else()
  message(STATUS "  ✗ noexcept specifier NOT available")
endif()

# Count total supported features
list(LENGTH CMAKE_CXX_COMPILE_FEATURES TOTAL_FEATURES)
message(STATUS "Total C++ features detected: ${TOTAL_FEATURES}")

# Show feature breakdown by standard
set(CXX98_COUNT 0)
set(CXX11_COUNT 0)
set(CXX14_COUNT 0)
set(CXX17_COUNT 0)
set(CXX20_COUNT 0)

foreach(feature ${CMAKE_CXX_COMPILE_FEATURES})
  if(feature MATCHES "cxx_std_98")
    math(EXPR CXX98_COUNT "${CXX98_COUNT} + 1")
  elseif(feature MATCHES "cxx_std_11|cxx_[a-z_]*[^0-9]$")
    math(EXPR CXX11_COUNT "${CXX11_COUNT} + 1") 
  elseif(feature MATCHES "cxx_std_14")
    math(EXPR CXX14_COUNT "${CXX14_COUNT} + 1")
  elseif(feature MATCHES "cxx_std_17")
    math(EXPR CXX17_COUNT "${CXX17_COUNT} + 1")
  elseif(feature MATCHES "cxx_std_20")
    math(EXPR CXX20_COUNT "${CXX20_COUNT} + 1")
  endif()
endforeach()

message(STATUS "Feature breakdown:")
message(STATUS "  C++98 features: ${CXX98_COUNT}")
message(STATUS "  C++11 features: ${CXX11_COUNT}")  
message(STATUS "  C++14 features: ${CXX14_COUNT}")
message(STATUS "  C++17 features: ${CXX17_COUNT}")
message(STATUS "  C++20 features: ${CXX20_COUNT}")

# Custom target to show all features
add_custom_target(show_features
  COMMAND ${CMAKE_COMMAND} -E echo "All detected C++ features:"
  COMMAND ${CMAKE_COMMAND} -E echo "${CMAKE_CXX_COMPILE_FEATURES}"
  VERBATIM
)