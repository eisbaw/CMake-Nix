#include "api.h"
#include <iostream>

extern "C" void shared_impl_function() {
    std::cout << "Shared library implementation (C++)" << std::endl;
}