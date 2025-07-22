cmake_minimum_required(VERSION 3.16)
project(TestModuleLibrary C)

# Create a MODULE library (plugin)
add_library(myplugin MODULE
    plugin.c
)

# Create executable
add_executable(plugin_demo
    main.c
)

# MODULE libraries are typically loaded at runtime, not linked
# But for testing purposes, we'll just verify they build correctly
