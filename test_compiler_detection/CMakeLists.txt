cmake_minimum_required(VERSION 3.28)
project(test_compiler_detection C CXX)

# Force specific compilers if provided
if(DEFINED ENV{TEST_C_COMPILER})
  set(CMAKE_C_COMPILER $ENV{TEST_C_COMPILER})
endif()

if(DEFINED ENV{TEST_CXX_COMPILER})
  set(CMAKE_CXX_COMPILER $ENV{TEST_CXX_COMPILER})
endif()

# Simple C executable
add_executable(c_program main.c)

# Simple C++ executable  
add_executable(cpp_program main.cpp)

# Mixed language executable
add_executable(mixed_program main.c helper.cpp)