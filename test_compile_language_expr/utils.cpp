#include <iostream>

#ifdef COMPILING_AS_CXX
#include "cxx_header.h"
#endif

#ifndef COMPILING_AS_C
// This should be defined for C++ files
#else
#error "C++ file incorrectly thinks it's C"
#endif

#ifdef COMPILING_C_OR_CXX
// This should be defined for both C and C++
#else
#error "COMPILING_C_OR_CXX not defined"
#endif

#ifdef USING_GXX_COMPILER
// This should be defined if using G++
#endif

extern "C" void cpp_function() {
    std::cout << "C++ function called" << std::endl;
    
#ifdef COMPILING_AS_CXX
    std::cout << "Successfully compiled as C++" << std::endl;
    cxx_function();
#endif
    
#ifdef USING_GXX_COMPILER
    std::cout << "Compiled with G++" << std::endl;
#endif
}