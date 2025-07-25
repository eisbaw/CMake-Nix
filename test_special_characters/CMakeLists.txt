cmake_minimum_required(VERSION 3.20)
project(TestSpecialCharacters C)

# Test various special characters in target names that CMake allows
# The Nix generator should properly handle these edge cases

# Target with dots (common in versioned names)
add_executable(my.test.app main.c)

# Target with plus sign
add_executable(my+app main.c)

# Test extremely long target name
add_executable(this_is_an_extremely_long_target_name_that_tests_whether_the_nix_generator_can_handle_very_long_identifiers_without_truncation_or_other_issues_when_generating_derivation_names main.c)

# Test target starting with number (should work but Nix attributes can't start with numbers)
add_executable(123_app main.c)

# Test all numbers (Nix will need to handle this specially)
add_executable(456789 main.c)

# Test with dashes (already tested but let's include)
add_executable(my-test-app main.c)

# Test with multiple underscores
add_executable(my___app main.c)

# Test starting with underscore
add_executable(_private_app main.c)

# Test ending with number
add_executable(app123 main.c)

# Test mixed case
add_executable(MyMixedCaseApp main.c)

# Test all caps
add_executable(MYAPP main.c)

# CMake rejects most special characters, which is good for security.
# The characters that were rejected include: @ = ( ) [ ] , ; space ! ? * $ # % & ' ` ~ | \ : < >
# This shows that CMake already has good input validation.