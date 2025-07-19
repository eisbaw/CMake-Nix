# Application in subdirectory
add_executable(calculator main.c)

# Link to library from other subdirectory
target_link_libraries(calculator mathlib)