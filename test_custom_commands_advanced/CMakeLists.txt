cmake_minimum_required(VERSION 3.20)
project(TestCustomCommandsAdvanced)

# Test 1: Custom command with WORKING_DIRECTORY
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/generated/config.h
    COMMAND ${CMAKE_COMMAND} -E echo "#define VERSION \"1.2.3\"" > config.h
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/generated
    COMMENT "Generating config.h in custom directory"
    VERBATIM
)

# Test 2: Custom command with multiple outputs
add_custom_command(
    OUTPUT 
        ${CMAKE_BINARY_DIR}/multi_output1.txt
        ${CMAKE_BINARY_DIR}/multi_output2.txt
        ${CMAKE_BINARY_DIR}/multi_output3.txt
    COMMAND ${CMAKE_COMMAND} -E echo "Output 1" > ${CMAKE_BINARY_DIR}/multi_output1.txt
    COMMAND ${CMAKE_COMMAND} -E echo "Output 2" > ${CMAKE_BINARY_DIR}/multi_output2.txt
    COMMAND ${CMAKE_COMMAND} -E echo "Output 3" > ${CMAKE_BINARY_DIR}/multi_output3.txt
    COMMENT "Generating multiple output files"
    VERBATIM
)

# Test 3: Complex command with shell operators (should be properly escaped)
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/shell_test.txt
    COMMAND ${CMAKE_COMMAND} -E echo "Testing shell operators: && || ; > < | \$VAR" > ${CMAKE_BINARY_DIR}/shell_test.txt
    COMMENT "Testing shell operator escaping"
    VERBATIM
)

# Test 4: Custom command with DEPENDS on other generated files
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/dependent.txt
    COMMAND ${CMAKE_COMMAND} -E cat multi_output1.txt multi_output2.txt > dependent.txt
    DEPENDS 
        ${CMAKE_BINARY_DIR}/multi_output1.txt
        ${CMAKE_BINARY_DIR}/multi_output2.txt
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Creating file that depends on other generated files"
    VERBATIM
)

# Test 5: Custom command with PRE_BUILD/PRE_LINK/POST_BUILD (attached to target)
add_executable(test_app main.cpp ${CMAKE_BINARY_DIR}/generated/config.h)
target_include_directories(test_app PRIVATE ${CMAKE_BINARY_DIR}/generated)

add_custom_command(
    TARGET test_app
    PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Pre-build step executed"
    COMMENT "Running pre-build custom command"
    VERBATIM
)

add_custom_command(
    TARGET test_app
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Post-build step executed"
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:test_app> ${CMAKE_BINARY_DIR}/test_app_copy
    COMMENT "Running post-build custom command"
    VERBATIM
)

# Test 6: Custom target that depends on all generated files
add_custom_target(generate_all
    DEPENDS
        ${CMAKE_BINARY_DIR}/generated/config.h
        ${CMAKE_BINARY_DIR}/multi_output1.txt
        ${CMAKE_BINARY_DIR}/multi_output2.txt
        ${CMAKE_BINARY_DIR}/multi_output3.txt
        ${CMAKE_BINARY_DIR}/shell_test.txt
        ${CMAKE_BINARY_DIR}/dependent.txt
    COMMENT "Target that generates all files"
)

# Make sure test_app depends on all generated files
add_dependencies(test_app generate_all)

# Test 7: Generator expressions in custom commands
set(MY_CUSTOM_FLAG "CUSTOM_VALUE")
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/genex_test.txt
    COMMAND ${CMAKE_COMMAND} -E echo "Config: $<CONFIG>, Flag: ${MY_CUSTOM_FLAG}" > ${CMAKE_BINARY_DIR}/genex_test.txt
    COMMENT "Testing generator expressions: $<CONFIG>"
    VERBATIM
)