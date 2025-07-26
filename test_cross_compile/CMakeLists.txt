cmake_minimum_required(VERSION 3.10)
project(test_cross_compile C)

# Simple program that shows target architecture
add_executable(cross_test src/main.c)

# Display cross-compilation settings
message(STATUS "CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")
message(STATUS "CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "CMAKE_CROSSCOMPILING: ${CMAKE_CROSSCOMPILING}")
message(STATUS "CMAKE_C_COMPILER: ${CMAKE_C_COMPILER}")

# Set some compile definitions based on target
if(CMAKE_CROSSCOMPILING)
    target_compile_definitions(cross_test PRIVATE CROSS_COMPILING)
    target_compile_definitions(cross_test PRIVATE TARGET_ARCH="${CMAKE_SYSTEM_PROCESSOR}")
else()
    target_compile_definitions(cross_test PRIVATE NATIVE_BUILD)
endif()