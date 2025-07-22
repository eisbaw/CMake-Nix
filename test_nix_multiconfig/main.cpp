#include <iostream>

int main() {
    std::cout << "Nix Multi-Config Test" << std::endl;
    std::cout << "Build configuration: " << BUILD_CONFIG << std::endl;
    
#ifdef DEBUG_MODE
    std::cout << "Running in DEBUG mode" << std::endl;
    std::cout << "Assertions enabled" << std::endl;
#endif

#ifdef RELEASE_MODE
    std::cout << "Running in RELEASE mode" << std::endl;
    std::cout << "Optimizations enabled" << std::endl;
#endif

    return 0;
}