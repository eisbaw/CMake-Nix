cmake_minimum_required(VERSION 3.20)
project(NixMultiConfigTest CXX)

# Add a simple executable
add_executable(multiconfig_app main.cpp)

# Add compile definitions that vary by configuration
target_compile_definitions(multiconfig_app PRIVATE
  $<$<CONFIG:Debug>:DEBUG_MODE>
  $<$<CONFIG:Release>:RELEASE_MODE>
  BUILD_CONFIG="$<CONFIG>"
)