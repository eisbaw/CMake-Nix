cmake_minimum_required(VERSION 3.20)
project(UnityBuildTest CXX)

# Create an executable with Unity Build enabled
add_executable(unity_app
    src/file1.cpp
    src/file2.cpp
    src/file3.cpp
    main.cpp
)

# Enable Unity Build
set_property(TARGET unity_app PROPERTY UNITY_BUILD ON)

# Optionally set Unity Build batch size
set_property(TARGET unity_app PROPERTY UNITY_BUILD_BATCH_SIZE 2)

# Create the same executable without Unity Build for comparison
add_executable(normal_app
    src/file1.cpp
    src/file2.cpp
    src/file3.cpp
    main.cpp
)