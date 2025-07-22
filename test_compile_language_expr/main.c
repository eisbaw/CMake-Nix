#include <stdio.h>

#ifdef COMPILING_AS_C
#include "c_header.h"
#endif

#ifndef COMPILING_AS_CXX
// This should be defined for C files
#else
#error "C file incorrectly thinks it's C++"
#endif

#ifdef COMPILING_C_OR_CXX
// This should be defined for both C and C++
#else
#error "COMPILING_C_OR_CXX not defined"
#endif

#ifdef USING_GCC_COMPILER
// This should be defined if using GCC
#endif

// Forward declaration
void cpp_function();

int main() {
    printf("C main function\n");
    
#ifdef COMPILING_AS_C
    printf("Successfully compiled as C\n");
    c_function();
#endif
    
#ifdef USING_GCC_COMPILER
    printf("Compiled with GCC\n");
#endif
    
    cpp_function();
    
    return 0;
}