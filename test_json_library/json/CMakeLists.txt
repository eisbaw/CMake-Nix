cmake_minimum_required(VERSION 3.16)
project(json VERSION 1.0.0)

# Create the JSON library
add_library(json
    src/json_parser.cpp
    src/json_writer.cpp
    src/json_value.cpp
)

# Set include directories
target_include_directories(json PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Add compile features
target_compile_features(json PUBLIC cxx_std_11)

# Install rules
install(TARGETS json
    EXPORT jsonTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(DIRECTORY include/json DESTINATION include)