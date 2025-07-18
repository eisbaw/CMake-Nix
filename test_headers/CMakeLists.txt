cmake_minimum_required(VERSION 3.20)
project(test_headers)

# Add include directory
include_directories(include)

# Set explicit compiler flags for testing
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")

# Add a custom definition to test defines
add_definitions(-DTEST_DEFINE=1)

# Create executable
add_executable(test_headers 
    src/main.c
    src/math_utils.c
)