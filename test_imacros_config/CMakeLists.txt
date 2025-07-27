cmake_minimum_required(VERSION 3.20)
project(test_imacros_config C)

# Generate a configuration header at configure time
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated_config.h"
)

# Create an executable that uses -imacros with the generated header
add_executable(test_imacros main.c)

# Add the -imacros flag to include the generated config file
target_compile_options(test_imacros PRIVATE
    -imacros ${CMAKE_CURRENT_BINARY_DIR}/generated_config.h
)

# Add include directory for build directory (in case of other generated files)
target_include_directories(test_imacros PRIVATE ${CMAKE_CURRENT_BINARY_DIR})