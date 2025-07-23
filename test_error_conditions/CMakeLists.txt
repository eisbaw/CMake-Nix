cmake_minimum_required(VERSION 3.20)
project(ErrorConditionTests C)

# Test circular dependency (should detect and warn)
add_library(circular_a STATIC circular_a.c)
add_library(circular_b STATIC circular_b.c)
target_link_libraries(circular_a circular_b)
target_link_libraries(circular_b circular_a)

# Test invalid target name characters
add_executable(test-with-dashes test_main.c)
add_executable(test_with_underscores test_main.c)

# Test very long target name
add_executable(very_long_target_name_that_might_cause_issues_with_nix_derivation_naming test_main.c)

# Test executable with circular dependency
add_executable(circular_test test_main.c)
target_link_libraries(circular_test circular_a)