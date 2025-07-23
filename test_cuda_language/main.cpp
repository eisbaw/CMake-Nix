#include <iostream>
#include <vector>
#include <cmath>
#include "cuda_kernels.h"

int main() {
    std::cout << "Mixed CUDA/C++ Vector Addition Test" << std::endl;
    std::cout << "====================================" << std::endl;
    
    const int n = 100;
    std::vector<float> a(n), b(n), c(n);
    
    // Initialize vectors
    for (int i = 0; i < n; i++) {
        a[i] = static_cast<float>(i);
        b[i] = static_cast<float>(i * 2);
    }
    
    // Call CUDA function
    vectorAdd(a.data(), b.data(), c.data(), n);
    
    // Verify results
    bool success = true;
    for (int i = 0; i < n; i++) {
        float expected = a[i] + b[i];
        if (std::abs(c[i] - expected) > 1e-5) {
            std::cout << "Error at index " << i << ": expected " 
                      << expected << ", got " << c[i] << std::endl;
            success = false;
            break;
        }
    }
    
    if (success) {
        std::cout << "All " << n << " vector additions correct!" << std::endl;
        std::cout << "Mixed CUDA/C++ test successful!" << std::endl;
    }
    
    return success ? 0 : 1;
}