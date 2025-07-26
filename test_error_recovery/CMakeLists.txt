cmake_minimum_required(VERSION 3.10)
project(test_error_recovery C)

# Test 1: Source file with syntax error
add_executable(syntax_error src/syntax_error.c)

# Test 2: Missing header dependency  
add_executable(missing_header src/missing_header.c)

# Test 3: Circular dependency (if supported)
add_library(circular_a STATIC src/circular_a.c)
add_library(circular_b STATIC src/circular_b.c)
target_link_libraries(circular_a circular_b)
target_link_libraries(circular_b circular_a)

# Test 4: Valid executable (should build)
add_executable(valid_program src/valid.c)

# Test 5: Source file that doesn't exist
add_executable(missing_source src/nonexistent.c)

# Test 6: Invalid target name characters (spaces)
# This should be handled gracefully
add_executable("target with spaces" src/valid2.c)