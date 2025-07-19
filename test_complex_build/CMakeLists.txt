cmake_minimum_required(VERSION 3.20)
project(ComplexBuildTest C CXX)

# Test mixed C/C++ compilation
add_executable(mixed_lang_app
    main.cpp
    helper.c
    utils.cpp
)

# Test multiple include directories
target_include_directories(mixed_lang_app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/include
)

# Test compile definitions
target_compile_definitions(mixed_lang_app PRIVATE
    VERSION_MAJOR=1
    VERSION_MINOR=0
    FEATURE_X_ENABLED
)

# Test compile options
target_compile_options(mixed_lang_app PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-std=c99>
    $<$<COMPILE_LANGUAGE:CXX>:-std=c++17>
    -Wall
    -Wextra
)

# Test static library with same complexity
add_library(complex_lib STATIC
    lib/complex.c
    lib/advanced.cpp
)

target_include_directories(complex_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/include
)

target_compile_definitions(complex_lib PUBLIC
    LIB_VERSION=1
    LIB_FEATURE_A
)

# Link the library to the executable
target_link_libraries(mixed_lang_app PRIVATE complex_lib)

# Test shared library
add_library(shared_complex SHARED
    shared/api.c
    shared/impl.cpp
)

target_include_directories(shared_complex PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/include
)

target_link_libraries(mixed_lang_app PRIVATE shared_complex)