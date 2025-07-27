cmake_minimum_required(VERSION 3.20)
project(test_external_config_deps C)

# Configure a header file
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/config.h"
)

# Configure another header that will be included by external source
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/external_config.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/external_config.h"
)

# Add executable with external source that depends on config-time generated headers
add_executable(test_app
  "${CMAKE_CURRENT_SOURCE_DIR}/main.c"
  "${CMAKE_SOURCE_DIR}/../external_src/external.c"  # External source outside project directory
)

# Include both source and binary directories for headers
target_include_directories(test_app PRIVATE 
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)

# Test that the Nix generator properly handles:
# 1. External source files (outside project directory)
# 2. Configuration-time generated headers
# 3. External sources depending on generated headers