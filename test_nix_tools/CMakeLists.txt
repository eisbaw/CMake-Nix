cmake_minimum_required(VERSION 3.20)
project(TestNixTools C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Simple library to test various Nix evaluation scenarios
add_library(simple_lib STATIC lib.cpp)

# Executable that uses the library
add_executable(simple_app main.cpp)
target_link_libraries(simple_app PRIVATE simple_lib)

# Add a shared library to test more complex scenarios
add_library(shared_lib SHARED shared.cpp)
set_target_properties(shared_lib PROPERTIES VERSION 1.0.0 SOVERSION 1)

# Executable using shared library
add_executable(shared_app shared_main.cpp)
target_link_libraries(shared_app PRIVATE shared_lib)

# Add custom command to test command handling
add_custom_command(
  OUTPUT generated.h
  COMMAND ${CMAKE_COMMAND} -E echo "#define GENERATED 1" > generated.h
  COMMENT "Generating header file"
)

add_executable(custom_app custom_main.cpp generated.h)
target_include_directories(custom_app PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# Install rules to test install handling
install(TARGETS simple_app shared_lib
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
)