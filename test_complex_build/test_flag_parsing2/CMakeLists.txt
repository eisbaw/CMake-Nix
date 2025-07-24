cmake_minimum_required(VERSION 3.20)
project(TestFlagParsing2 CXX)

# Create a simple executable
add_executable(test_app main.cpp)

# Add target-specific compile options as a single string
target_compile_options(test_app PRIVATE "-fPIC -pthread")