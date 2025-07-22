cmake_minimum_required(VERSION 3.20)
project(TestInterfaceLibrary)

# Create an interface library
add_library(common_flags INTERFACE)
target_compile_definitions(common_flags INTERFACE USE_COMMON_FLAGS)
target_compile_options(common_flags INTERFACE -Wall -Wextra)
target_include_directories(common_flags INTERFACE include)

# Create an executable that uses the interface library
add_executable(app main.c)
target_link_libraries(app PRIVATE common_flags)