cmake_minimum_required(VERSION 3.20)
project(PreprocessorTest C)

# Create library with various preprocessor definitions
add_library(config_lib STATIC config.c)

# Add comprehensive compile definitions
target_compile_definitions(config_lib PRIVATE
    VERSION_MAJOR=2
    VERSION_MINOR=1
    VERSION_PATCH=0
    BUILD_NUMBER=42
    FEATURE_LOGGING=1
    FEATURE_NETWORKING=0
    MAX_CONNECTIONS=100
    DEFAULT_TIMEOUT=30
    CONFIG_STRING=production
    DEBUG_LEVEL=2
)

# Create executable with conditional compilation
add_executable(preprocessor_app main.c)
target_link_libraries(preprocessor_app PRIVATE config_lib)

# Add configuration-specific definitions
# Use simpler approach to avoid quoting issues
target_compile_definitions(preprocessor_app PRIVATE
    APP_NAME=PreprocessorTest
    COMPILE_DATE=DATE
)

# Add debug-specific definitions for Debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(preprocessor_app PRIVATE
        DEBUG_BUILD=1
        ENABLE_ASSERTS=1
    )
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(preprocessor_app PRIVATE
        OPTIMIZE_BUILD=1
        DISABLE_LOGGING=1
    )
endif()

# Test different include directories
include_directories(include)
target_include_directories(preprocessor_app PRIVATE include)