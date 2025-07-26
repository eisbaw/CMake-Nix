cmake_minimum_required(VERSION 3.20)
project(TestCircularDeps)

# Test 1: Direct circular dependency (should be detected and error)
add_library(libA STATIC libA.cpp)
add_library(libB STATIC libB.cpp)

# Create circular dependency - CMake should detect this
target_link_libraries(libA PRIVATE libB)
target_link_libraries(libB PRIVATE libA)

# Test 2: Indirect circular dependency through three libraries
add_library(libX STATIC libX.cpp)
add_library(libY STATIC libY.cpp)
add_library(libZ STATIC libZ.cpp)

target_link_libraries(libX PRIVATE libY)
target_link_libraries(libY PRIVATE libZ)
target_link_libraries(libZ PRIVATE libX)

# Test 3: Valid dependency chain (no cycle)
add_library(lib1 STATIC lib1.cpp)
add_library(lib2 STATIC lib2.cpp)
add_library(lib3 STATIC lib3.cpp)

target_link_libraries(lib1 PRIVATE lib2)
target_link_libraries(lib2 PRIVATE lib3)
# lib3 has no dependencies - this is valid

# Test executable that uses the valid chain
add_executable(valid_app main.cpp)
target_link_libraries(valid_app PRIVATE lib1)