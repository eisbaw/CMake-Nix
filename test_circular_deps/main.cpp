#include <iostream>

extern int func1();

int main() {
    std::cout << "Testing valid dependency chain..." << std::endl;
    std::cout << "Result: " << func1() << std::endl;
    std::cout << "Test passed!" << std::endl;
    return 0;
}