cmake_minimum_required(VERSION 3.20)
project(CMakeSelfHostTest C CXX)

# Simplified CMake self-hosting test
# This creates a minimal subset of CMake targets to test the Nix generator on complex real-world code

# Include system headers we need
find_package(Threads REQUIRED)

# Create a small subset of CMake's core files for testing
add_library(cmake_core STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../Source/cmSystemTools.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/../Source/cmStringAlgorithms.cxx
    ${CMAKE_CURRENT_SOURCE_DIR}/../Source/cmValue.cxx
)

target_include_directories(cmake_core PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../Source
    ${CMAKE_CURRENT_SOURCE_DIR}/../Bootstrap.cmk
)

target_compile_definitions(cmake_core PRIVATE
    CMAKE_BUILD_WITH_CMAKE
    KWSYS_NAMESPACE=cmsys
)

# Create a test executable that uses the core library
add_executable(cmake_test_app
    test_main.cxx
)

target_link_libraries(cmake_test_app PRIVATE 
    cmake_core
    Threads::Threads
)

target_include_directories(cmake_test_app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../Source
    ${CMAKE_CURRENT_SOURCE_DIR}/../Bootstrap.cmk
)