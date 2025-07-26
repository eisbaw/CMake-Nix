#include <iostream>
#include <string>

// External function from utils.cpp
std::string getOptimizationLevel();

// External function from lib.cpp
std::string getConfigName();

int main() {
    std::cout << "Multi-Config Edge Case Test" << std::endl;
    std::cout << "===========================" << std::endl;
    
    // Check compile-time configuration
#ifdef DEBUG_MODE
    std::cout << "Build mode: DEBUG" << std::endl;
    std::cout << "Optimization: Debug (-O0 -g)" << std::endl;
#elif defined(RELEASE_MODE)
    std::cout << "Build mode: RELEASE" << std::endl;
    std::cout << "Optimization: Release (-O3)" << std::endl;
#elif defined(RELWITHDEBINFO_MODE)
    std::cout << "Build mode: RELWITHDEBINFO" << std::endl;
    std::cout << "Optimization: RelWithDebInfo (-O2 -g)" << std::endl;
#elif defined(MINSIZEREL_MODE)
    std::cout << "Build mode: MINSIZEREL" << std::endl;
    std::cout << "Optimization: MinSizeRel (-Os)" << std::endl;
#else
    std::cout << "Build mode: UNKNOWN" << std::endl;
#endif

    std::cout << "Config from macro: " << BUILD_CONFIG << std::endl;
    std::cout << "Config from library: " << getConfigName() << std::endl;
    std::cout << "Optimization check: " << getOptimizationLevel() << std::endl;
    
    // Size optimization test - should be smaller in MinSizeRel
    volatile int array[1000];
    for (int i = 0; i < 1000; ++i) {
        array[i] = i * i;
    }
    
    std::cout << "✅ Test passed!" << std::endl;
    return 0;
}