cmake_minimum_required(VERSION 3.28)
project(test_shared_library C)

# Simple shared library
add_library(mylib SHARED lib.c)

# Shared library with versioning
add_library(versioned_lib SHARED versioned.c)
set_target_properties(versioned_lib PROPERTIES
  VERSION 1.2.3
  SOVERSION 1
)

# Executable linking to shared library
add_executable(app main.c)
target_link_libraries(app mylib)

# Test mixed static/shared linking
add_library(static_helper STATIC helper.c)
add_executable(mixed_app mixed.c)
target_link_libraries(mixed_app mylib static_helper)