cmake_minimum_required(VERSION 3.10)
project(test_fmt_library)

# Simple example using our simplified fmt header
add_executable(fmt_example main.cpp)
target_compile_features(fmt_example PRIVATE cxx_std_11)