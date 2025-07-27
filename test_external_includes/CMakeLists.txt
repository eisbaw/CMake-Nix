cmake_minimum_required(VERSION 3.20)
project(TestExternalIncludes C)

add_executable(test_app src/main.c)

# Add an external include directory (outside project tree)
target_include_directories(test_app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../external/mylib/include)