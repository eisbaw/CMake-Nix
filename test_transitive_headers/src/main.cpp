#include "level1.h"  // Only directly includes level1.h

int main() {
    std::cout << "Testing transitive header dependencies" << std::endl;
    std::cout << "Deep constant value: " << DEEP_CONSTANT << std::endl;
    level1_function();
    return 0;
}