cmake_minimum_required(VERSION 3.20)
project(TestCustomSimple)

# Create build directory for generated files
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# Custom command that generates a file in build directory (no subdirectory)
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/config.h
  COMMAND ${CMAKE_COMMAND} -E echo "#define VERSION 1.0" > ${CMAKE_CURRENT_BINARY_DIR}/config.h
  COMMENT "Generating config.h"
  VERBATIM
)

# Source file
file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/main.c "
#include \"config.h\"
#include <stdio.h>

int main() {
    printf(\"Version: %s\\n\", VERSION);
    return 0;
}
")

# Add executable
add_executable(test_app main.c ${CMAKE_CURRENT_BINARY_DIR}/config.h)

# Include build directory for generated header
target_include_directories(test_app PRIVATE ${CMAKE_CURRENT_BINARY_DIR})