cmake_minimum_required(VERSION 3.16)
project(TestObjectLibrary CXX)

# Create an OBJECT library
add_library(math_objects OBJECT
    src/add.cpp
    src/multiply.cpp
)

# Set include directories for the object library
target_include_directories(math_objects PUBLIC include)

# Create executable that uses the OBJECT library
add_executable(calculator
    src/main.cpp
    $<TARGET_OBJECTS:math_objects>
)

# The executable should inherit include directories
target_include_directories(calculator PRIVATE include)

# Create a static library that also uses the OBJECT library
add_library(math_static STATIC
    $<TARGET_OBJECTS:math_objects>
)

# Create a shared library that uses the OBJECT library
add_library(math_shared SHARED
    $<TARGET_OBJECTS:math_objects>
)
