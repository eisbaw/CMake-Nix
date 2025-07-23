cmake_minimum_required(VERSION 3.20)
project(test_explicit_sources C)

# Enable explicit source derivations
set(CMAKE_NIX_EXPLICIT_SOURCES ON CACHE BOOL "Use explicit source derivations")

# Create a simple executable with multiple source files
add_executable(explicit_test
    main.c
    utils/helper.c
)

# Add include directory
target_include_directories(explicit_test PRIVATE include)