cmake_minimum_required(VERSION 3.28)
project(test_find_package C)

# Test with common packages that should be available in Nix
find_package(Threads REQUIRED)
find_package(ZLIB)

# Test basic find_package that might not exist
find_package(OpenGL)

# Simple executable using Threads (built into compiler)
add_executable(threaded_app threaded.c)
target_link_libraries(threaded_app Threads::Threads)

# Simple executable using ZLIB (if found)
if(ZLIB_FOUND)
    add_executable(compress_app compress.c)
    target_link_libraries(compress_app ZLIB::ZLIB)
    message(STATUS "ZLIB found, building compress_app")
else()
    message(STATUS "ZLIB not found, skipping compress_app")
endif()

# Conditional OpenGL usage
if(OpenGL_FOUND)
    add_executable(opengl_app opengl.c)
    target_link_libraries(opengl_app OpenGL::GL)
    message(STATUS "OpenGL found, building opengl_app")
else()
    message(STATUS "OpenGL not found, skipping opengl_app")
endif()