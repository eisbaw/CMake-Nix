cmake_minimum_required(VERSION 3.28)
project(TestMultiConfig)

# Define configuration types
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)

# Add a simple executable
add_executable(myapp main.cpp)

# Set per-config compile definitions
target_compile_definitions(myapp PRIVATE
  $<$<CONFIG:Debug>:DEBUG_MODE>
  $<$<CONFIG:Release>:RELEASE_MODE>
  BUILD_CONFIG="${CMAKE_CFG_INTDIR}"
)

# Set per-config compile options
target_compile_options(myapp PRIVATE
  $<$<CONFIG:Debug>:-O0 -g>
  $<$<CONFIG:Release>:-O3>
)
