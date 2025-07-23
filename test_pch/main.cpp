// This file should benefit from precompiled headers
// Note: Standard headers are in PCH, so we don't need to include them

#include "utils.h"
#include "math_helpers.h"
#include "common.h"

int main() {
    print_header();
    
    std::vector<int> numbers = {1, 2, 3, 4, 5};
    
    std::cout << "Original numbers: ";
    print_vector(numbers);
    
    double_vector(numbers);
    std::cout << "Doubled numbers: ";
    print_vector(numbers);
    
    int sum = sum_vector(numbers);
    std::cout << "Sum: " << sum << std::endl;
    
    std::cout << "PCH test completed successfully!" << std::endl;
    
    return 0;
}