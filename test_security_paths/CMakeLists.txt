cmake_minimum_required(VERSION 3.20)
project(SecurityPathTest C)

# Test various path characters that could be problematic
# Note: Some characters cannot be used in actual file names on many filesystems

# Simple executable with safe name
add_executable(safe_target main.c)

# Test custom command with quotes in arguments (should be properly escaped)
add_custom_target(test_quotes ALL
    COMMAND ${CMAKE_COMMAND} -E echo "Testing quote \" handling"
    COMMAND ${CMAKE_COMMAND} -E echo 'Single quote test'
    VERBATIM
)

# Test paths with spaces (common security issue)
set(SPACE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dir with spaces")
file(MAKE_DIRECTORY "${SPACE_DIR}")
file(WRITE "${SPACE_DIR}/test.c" "int func() { return 42; }")

add_library(space_lib STATIC "${SPACE_DIR}/test.c")

# Test dollar signs in paths (could break Nix expressions)
add_custom_target(test_dollar ALL
    COMMAND ${CMAKE_COMMAND} -E echo "Testing dollar \$\$ signs"
    VERBATIM
)

# Test backticks (command substitution risk)
add_custom_target(test_backtick ALL
    COMMAND ${CMAKE_COMMAND} -E echo "Testing backtick \` handling"
    VERBATIM
)

# Test target with dashes and underscores (should be safe)
add_executable(test-with-dashes_and_underscores main.c)

# Test very long names
add_executable(this_is_a_very_long_target_name_that_might_cause_issues_with_path_length_limits_in_some_systems main.c)