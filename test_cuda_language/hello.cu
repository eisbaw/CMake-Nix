#include <iostream>
#include <cuda_runtime.h>

__global__ void helloKernel() {
    printf("Hello from CUDA kernel! Thread %d of Block %d\n", 
           threadIdx.x, blockIdx.x);
}

int main() {
    std::cout << "CUDA Hello World Test" << std::endl;
    std::cout << "====================" << std::endl;
    
    // Get device properties
    int deviceCount = 0;
    cudaGetDeviceCount(&deviceCount);
    
    if (deviceCount == 0) {
        std::cout << "No CUDA devices found. Running CPU-only test." << std::endl;
        std::cout << "CUDA compilation test successful!" << std::endl;
        return 0;
    }
    
    std::cout << "Found " << deviceCount << " CUDA device(s)" << std::endl;
    
    // Launch kernel
    helloKernel<<<2, 4>>>();
    cudaDeviceSynchronize();
    
    std::cout << "CUDA test successful!" << std::endl;
    
    return 0;
}