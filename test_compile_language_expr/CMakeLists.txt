cmake_minimum_required(VERSION 3.20)
project(TestCompileLanguageExpr C CXX)

# Create a mixed C/C++ executable
add_executable(mixed_lang_test
    main.c
    utils.cpp
)

# Test COMPILE_LANGUAGE generator expressions
target_compile_definitions(mixed_lang_test PRIVATE
    $<$<COMPILE_LANGUAGE:C>:COMPILING_AS_C>
    $<$<COMPILE_LANGUAGE:CXX>:COMPILING_AS_CXX>
    $<$<COMPILE_LANGUAGE:C,CXX>:COMPILING_C_OR_CXX>
)

# Test language-specific compile options
target_compile_options(mixed_lang_test PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-std=c++11>
    $<$<COMPILE_LANGUAGE:C>:-std=c99>
)

# Test language-specific include directories
target_include_directories(mixed_lang_test PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/cxx_includes>
    $<$<COMPILE_LANGUAGE:C>:${CMAKE_CURRENT_SOURCE_DIR}/c_includes>
)

# Test COMPILE_LANG_AND_ID expressions
target_compile_definitions(mixed_lang_test PRIVATE
    $<$<COMPILE_LANG_AND_ID:CXX,GNU>:USING_GXX_COMPILER>
    $<$<COMPILE_LANG_AND_ID:C,GNU>:USING_GCC_COMPILER>
)