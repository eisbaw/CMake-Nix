cmake_minimum_required(VERSION 3.20)
project(TestExternalTools C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Test ExternalProject_Add
include(ExternalProject)

# Add an external project - a simple header-only library
ExternalProject_Add(simple_external
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.2
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
  UPDATE_DISCONNECTED TRUE
)

# Get the source directory for the external project
ExternalProject_Get_Property(simple_external SOURCE_DIR)

# Create an interface library for the external project
add_library(json_external INTERFACE)
add_dependencies(json_external simple_external)
target_include_directories(json_external INTERFACE ${SOURCE_DIR}/single_include)

# Test FetchContent
include(FetchContent)

# Fetch a simple header-only library
FetchContent_Declare(
  fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG 10.1.1
)

FetchContent_MakeAvailable(fmt)

# Create a test executable that uses both external project and fetched content
add_executable(external_test main.cpp)
target_link_libraries(external_test PRIVATE json_external fmt::fmt-header-only)

# Add custom command to show dependencies
add_custom_command(TARGET external_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E echo "External tools test built successfully"
)