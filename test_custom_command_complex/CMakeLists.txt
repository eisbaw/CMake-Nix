cmake_minimum_required(VERSION 3.15)
project(TestCustomCommandComplex C)

# Test 1: Multiple custom commands with dependencies
add_custom_command(
    OUTPUT step1.h
    COMMAND ${CMAKE_COMMAND} -E echo "#define STEP1_VALUE 100" > step1.h
    COMMENT "Generating step1.h"
    VERBATIM
)

add_custom_command(
    OUTPUT step2.h
    DEPENDS step1.h
    COMMAND ${CMAKE_COMMAND} -E echo "#define STEP2_VALUE (STEP1_VALUE * 2)" > step2.h
    COMMENT "Generating step2.h (depends on step1.h)"
    VERBATIM
)

add_custom_command(
    OUTPUT generated_complex.c
    DEPENDS step1.h step2.h generated_template.c.in
    COMMAND ${CMAKE_COMMAND} -E copy generated_template.c.in generated_complex.c
    COMMENT "Generating generated_complex.c"
    VERBATIM
)

# Create an executable that uses the generated source
add_executable(complex_app main_complex.c generated_complex.c)
target_include_directories(complex_app PRIVATE ${CMAKE_CURRENT_BINARY_DIR})