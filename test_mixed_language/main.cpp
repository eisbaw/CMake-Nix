#include "cpp_utils.hpp"
#include "c_math.h"
#include <iostream>
#include <vector>

// Declare C function
extern "C" void test_c_functions(void);

int main() {
    std::cout << "Mixed Language Test Application\n";
    std::cout << "===============================\n\n";
    
    // Test C functions directly
    test_c_functions();
    std::cout << std::endl;
    
    // Test C++ wrapper around C functions
    std::cout << "Testing C++ wrapper:\n";
    CCalculator calc;
    std::cout << "calc.add(15, 25) = " << calc.add(15, 25) << std::endl;
    std::cout << "calc.multiply(6, 8) = " << calc.multiply(6, 8) << std::endl;
    std::cout << "calc.divide(100.0, 3.0) = " << calc.divide(100.0, 3.0) << std::endl;
    std::cout << std::endl;
    
    // Test pure C++ functionality
    std::cout << "Testing pure C++ functionality:\n";
    auto fib = MathUtils::fibonacci(8);
    std::cout << "Fibonacci sequence (8 numbers): ";
    for (size_t i = 0; i < fib.size(); ++i) {
        std::cout << fib[i];
        if (i < fib.size() - 1) std::cout << ", ";
    }
    std::cout << std::endl;
    
    std::cout << MathUtils::format_result(42) << std::endl;
    std::cout << "2^10 = " << MathUtils::power(2.0, 10) << std::endl;
    
    std::cout << "\nMixed language test completed successfully!\n";
    return 0;
}