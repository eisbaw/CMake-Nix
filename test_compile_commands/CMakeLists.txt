cmake_minimum_required(VERSION 3.20)
project(CompileCommandsTest CXX)

# Enable compile commands export
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Simple executable
add_executable(simple_app main.cpp)