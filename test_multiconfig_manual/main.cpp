#include <iostream>

int main() {
    std::cout << "Multi-Config Test" << std::endl;
#ifdef DEBUG_MODE
    std::cout << "DEBUG configuration" << std::endl;
#endif
#ifdef RELEASE_MODE
    std::cout << "RELEASE configuration" << std::endl;
#endif
    return 0;
}
