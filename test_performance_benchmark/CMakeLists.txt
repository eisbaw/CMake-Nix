cmake_minimum_required(VERSION 3.20)
project(PerformanceBenchmark CXX)

# Performance benchmark test for CMake Nix generator
# Compares generation time against other generators

set(CMAKE_CXX_STANDARD 17)

# Generate a configurable number of source files
set(NUM_SOURCE_FILES 100 CACHE STRING "Number of source files to generate")

# Generate source files
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/generated)
foreach(i RANGE 1 ${NUM_SOURCE_FILES})
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/generated/source${i}.cpp
    "#include <iostream>\n"
    "void function${i}() {\n"
    "  std::cout << \"Function ${i}\" << std::endl;\n"
    "}\n"
  )
endforeach()

# Create a library with all generated sources
file(GLOB GENERATED_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/generated/*.cpp)
add_library(benchmark_lib STATIC ${GENERATED_SOURCES})

# Create main executable
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/main.cpp
  "#include <iostream>\n"
  "int main() {\n"
  "  std::cout << \"Performance benchmark with ${NUM_SOURCE_FILES} files\" << std::endl;\n"
  "  return 0;\n"
  "}\n"
)

add_executable(benchmark_app ${CMAKE_CURRENT_BINARY_DIR}/main.cpp)
target_link_libraries(benchmark_app PRIVATE benchmark_lib)