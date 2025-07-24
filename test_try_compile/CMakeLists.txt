cmake_minimum_required(VERSION 3.20)
project(TryCompileTest C)

# Simple try_compile test
include(CheckCSourceCompiles)

check_c_source_compiles("
int main() {
    return 0;
}
" SIMPLE_PROGRAM_COMPILES)

if(SIMPLE_PROGRAM_COMPILES)
    message(STATUS "Simple program compiles successfully")
else()
    message(FATAL_ERROR "Simple program failed to compile")
endif()