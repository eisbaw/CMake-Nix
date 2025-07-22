cmake_minimum_required(VERSION 3.20)
project(TransitiveHeadersTest CXX)

# Add include directory
include_directories(include)

# Create executable
add_executable(transitive_test src/main.cpp)