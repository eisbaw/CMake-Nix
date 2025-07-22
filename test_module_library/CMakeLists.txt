cmake_minimum_required(VERSION 3.16)
project(TestModuleLibrary CXX)

# Enable position independent code for plugins
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Create a shared library that will load plugins
add_library(plugin_host SHARED
    src/plugin_host.cpp
)

# Create MODULE libraries (plugins)
add_library(plugin_math MODULE
    src/plugin_math.cpp
)

add_library(plugin_string MODULE  
    src/plugin_string.cpp
)

# Create executable that uses the plugin host
add_executable(plugin_demo
    src/main.cpp
)

target_link_libraries(plugin_demo PRIVATE plugin_host)

# Set include directories
target_include_directories(plugin_host PUBLIC include)
target_include_directories(plugin_math PRIVATE include)
target_include_directories(plugin_string PRIVATE include)
target_include_directories(plugin_demo PRIVATE include)
EOF < /dev/null
