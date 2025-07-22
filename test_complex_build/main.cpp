#include <iostream>
#include "helpers.h"
#include "utils.hpp"
#include "complex_lib.h"
#include "shared_api.h"

int main() {
    std::cout << "Complex build test - C++ main" << std::endl;
    
    // Test version display
    print_version();
    
    // Test helper function
    int result = helper_function(5);
    std::cout << "Helper result: " << result << std::endl;
    
    // Test utils
    std::string feature_str = get_feature_string();
    std::cout << feature_str << std::endl;
    int sum = utility_compute(3, 4);
    
    // Test complex library
    int complex_result = complex_function(10);
    std::cout << "Complex function result: " << complex_result << std::endl;
    int advanced_result = advanced_process(7);
    std::cout << "Advanced process result: " << advanced_result << std::endl;
    
    // Test shared library
    shared_init();
    int shared_result = shared_compute(12, 8);
    std::cout << "Shared compute result: " << shared_result << std::endl;
    
    std::cout << "All tests completed successfully!" << std::endl;
    
    return 0;
}