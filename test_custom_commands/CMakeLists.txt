cmake_minimum_required(VERSION 3.15)
project(TestCustomCommands C)

# Custom command to generate a source file
add_custom_command(
    OUTPUT generated.c
    COMMAND ${CMAKE_COMMAND} -E echo "// Generated file" > generated.c
    COMMAND ${CMAKE_COMMAND} -E echo "int get_generated_value() { return 42; }" >> generated.c
    COMMENT "Generating generated.c"
    VERBATIM
)

# Create an executable that uses the generated source
add_executable(app main.c generated.c)