cmake_minimum_required(VERSION 3.20)
project(TestCustomCommandSubdir)

# Create a subdirectory for generated files
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/generated)

# Custom command that generates a file in a subdirectory
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/generated/config.h
  COMMAND ${CMAKE_COMMAND} -E echo "#define VERSION 1.0" > ${CMAKE_CURRENT_BINARY_DIR}/generated/config.h
  COMMENT "Generating config.h in subdirectory"
)

# Create source file that will use the generated header
file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/main.c "
#include \"generated/config.h\"
#include <stdio.h>

int main() {
    printf(\"Version: %s\\n\", VERSION);
    return 0;
}
")

# Add executable that depends on the generated file
add_executable(test_app main.c ${CMAKE_CURRENT_BINARY_DIR}/generated/config.h)

# Include the directory with generated files
target_include_directories(test_app PRIVATE ${CMAKE_CURRENT_BINARY_DIR})