#include <iostream>
#include "helper.h"
#include "utils.h"
#include "complex.h"
#include "api.h"

int main() {
    std::cout << "Complex build test - C++ main" << std::endl;
    
    helper_function();
    utils_function();
    complex_lib_function();
    shared_api_function();
    
    std::cout << "Version: " << VERSION_MAJOR << "." << VERSION_MINOR << std::endl;
    
    return 0;
}