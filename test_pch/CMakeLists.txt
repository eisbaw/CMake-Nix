cmake_minimum_required(VERSION 3.16)
project(test_pch CXX)

# Enable precompiled headers
set(CMAKE_CXX_STANDARD 11)

# Create main executable
add_executable(pch_test
    main.cpp
    utils.cpp
    math_helpers.cpp
)

# Set precompiled headers
target_precompile_headers(pch_test
    PRIVATE
        # Standard library headers only for now
        <iostream>
        <vector>
        <string>
        <algorithm>
)