cmake_minimum_required(VERSION 3.20)
project(GeneratorExpressionsTest C CXX)

# Create library with complex generator expressions
add_library(mylib STATIC src/mylib.cpp)

# Test various generator expressions
target_include_directories(mylib PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

# Test conditional compilation based on build type
target_compile_definitions(mylib PRIVATE
  $<$<CONFIG:Debug>:DEBUG_MODE>
  $<$<CONFIG:Release>:RELEASE_MODE>
  $<$<CONFIG:RelWithDebInfo>:RELWITHDEBINFO_MODE>
  $<$<CONFIG:MinSizeRel>:MINSIZEREL_MODE>
)

# Test language-specific flags
target_compile_options(mylib PRIVATE
  $<$<COMPILE_LANGUAGE:CXX>:-std=c++17>
  $<$<CONFIG:Debug>:-g>
  $<$<CONFIG:Release>:-O3>
)

# Test conditional sources
add_executable(myapp 
  src/main.cpp
  $<$<CONFIG:Debug>:src/debug_utils.cpp>
  $<$<CONFIG:Release>:src/release_utils.cpp>
  $<$<AND:$<NOT:$<CONFIG:Debug>>,$<NOT:$<CONFIG:Release>>>:src/default_utils.cpp>
)

# Test target-specific properties
target_link_libraries(myapp PRIVATE 
  mylib
  $<$<PLATFORM_ID:Linux>:pthread>
  $<$<PLATFORM_ID:Darwin>:pthread>
)

# Test install expressions
install(TARGETS mylib myapp
  RUNTIME DESTINATION $<$<CONFIG:Debug>:debug/bin>$<$<CONFIG:Release>:bin>
  LIBRARY DESTINATION $<$<CONFIG:Debug>:debug/lib>$<$<CONFIG:Release>:lib>
  ARCHIVE DESTINATION $<$<CONFIG:Debug>:debug/lib>$<$<CONFIG:Release>:lib>
)

# Test file generation with generator expressions
file(GENERATE
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/config_$<CONFIG>.h"
  CONTENT "#define BUILD_TYPE \"$<CONFIG>\"\n#define COMPILER_ID \"${CMAKE_CXX_COMPILER_ID}\"\n"
)

# Test custom command with generator expressions
add_custom_command(
  TARGET myapp POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E echo "Built myapp for $<CONFIG> configuration"
  VERBATIM
)