cmake_minimum_required(VERSION 3.20)
project(TestFlagParsing CXX)

# Set multi-flag strings that need to be split
set(CMAKE_CXX_FLAGS "-std=c++17 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Create a simple executable
add_executable(test_app main.cpp)

# Add some target-specific compile options
target_compile_options(test_app PRIVATE "-fPIC -pthread")