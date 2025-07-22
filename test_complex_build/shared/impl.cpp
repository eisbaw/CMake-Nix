#include "shared_api.h"
#include <iostream>
#include <vector>

class SharedProcessor {
private:
    std::vector<int> cache;
    
public:
    SharedProcessor() {
        std::cout << "SharedProcessor initialized (C++)" << std::endl;
    }
    
    int process(int a, int b) {
        cache.push_back(a);
        cache.push_back(b);
        return a * b + static_cast<int>(cache.size());
    }
};

// C++ implementation that can be called from C code
extern "C" int shared_advanced_compute(int a, int b) {
    static SharedProcessor processor;
    return processor.process(a, b);
}