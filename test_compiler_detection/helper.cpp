#include <iostream>

extern "C" void print_from_cpp() {
    std::cout << "Helper function from C++" << std::endl;
}