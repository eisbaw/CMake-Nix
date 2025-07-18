cmake_minimum_required(VERSION 3.20)
project(explicit_headers_test C)

# Explicitly specify header directories (not using include_directories)
# This forces us to be explicit about dependencies

# Create an interface library for headers to make dependencies explicit
add_library(core_headers INTERFACE)
target_include_directories(core_headers INTERFACE headers)

# Add custom compile definitions
add_definitions(-DDEBUG_MEMORY=1)

# Set configuration-specific flags
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG_BUILD=1 -DDEBUG_MEMORY=1")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG -DRELEASE_BUILD=1")

# Create executable
add_executable(ecs_app
    src/main.c
    src/entity.c
    src/memory.c
)

# Link with header library to establish dependencies
target_link_libraries(ecs_app PRIVATE core_headers)

# Explicitly specify header dependencies for each source file
# This tests manual dependency specification
set_source_files_properties(src/entity.c PROPERTIES
    OBJECT_DEPENDS "headers/core/entity.h;headers/core/base.h;headers/platform/memory.h"
)

set_source_files_properties(src/memory.c PROPERTIES  
    OBJECT_DEPENDS "headers/platform/memory.h;headers/core/base.h"
)

set_source_files_properties(src/main.c PROPERTIES
    OBJECT_DEPENDS "headers/core/entity.h;headers/platform/memory.h;headers/core/base.h"
)

# Test: Changes to any header should trigger rebuild of dependent sources
# Test: Manual OBJECT_DEPENDS should be respected by Nix generator
# Test: Interface library dependencies should be tracked