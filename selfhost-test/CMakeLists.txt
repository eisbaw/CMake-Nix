cmake_minimum_required(VERSION 3.20)
project(CMakeSelfHostDemo CXX)

# Simple demonstration of CMake self-hosting with Nix generator
add_executable(selfhost_demo simple_demo.cpp)

# Add a library to show more complex builds work
add_library(demo_utils STATIC demo_utils.cpp)
target_link_libraries(selfhost_demo PRIVATE demo_utils)

# Install demonstration
install(TARGETS selfhost_demo demo_utils
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)