cmake_minimum_required(VERSION 3.16)
project(TestJsonLibrary CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Create a simple JSON library as a subdirectory
add_subdirectory(json)

# Create test executable
add_executable(json_example src/main.cpp)
target_link_libraries(json_example PRIVATE json)