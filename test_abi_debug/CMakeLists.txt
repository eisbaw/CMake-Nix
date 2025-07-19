cmake_minimum_required(VERSION 3.20)
project(ABIDebugTest C)

# Enable debug output for try-compile
set(CMAKE_VERBOSE_MAKEFILE ON)

# This should trigger ABI detection
enable_language(C)

# Print ABI detection results
message(STATUS "=== ABI Detection Results ===")
message(STATUS "CMAKE_C_ABI_COMPILED: ${CMAKE_C_ABI_COMPILED}")
message(STATUS "CMAKE_SIZEOF_VOID_P: ${CMAKE_SIZEOF_VOID_P}")
message(STATUS "CMAKE_C_SIZEOF_DATA_PTR: ${CMAKE_C_SIZEOF_DATA_PTR}")
message(STATUS "CMAKE_C_BYTE_ORDER: ${CMAKE_C_BYTE_ORDER}")
message(STATUS "CMAKE_C_COMPILER_ABI: ${CMAKE_C_COMPILER_ABI}")
message(STATUS "CMAKE_C_COMPILER_ARCHITECTURE_ID: ${CMAKE_C_COMPILER_ARCHITECTURE_ID}")

# Simple test executable
add_executable(abi_test main.c)