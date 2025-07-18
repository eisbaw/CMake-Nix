cmake_minimum_required(VERSION 3.20)
project(implicit_headers_test C)

# Use include_directories - CMake should implicitly track all headers
include_directories(include)

# Set some compile flags to test configuration
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG_BUILD=1")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG -DRELEASE_BUILD=1")

# Create executable with multiple source files
# CMake should automatically detect all header dependencies
add_executable(calc_app
    src/main.c
    src/calculator.c 
    src/logger.c
)

# Test: Any change to config.h should trigger rebuild of all sources
# Test: Change to calculator.h should trigger rebuild of calculator.c and logger.c
# Test: Change to logger.h should only trigger rebuild of logger.c