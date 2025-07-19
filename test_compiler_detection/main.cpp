#include <iostream>

int main() {
    std::cout << "C++ program compiled with detected compiler" << std::endl;
    #ifdef __GNUC__
    std::cout << "Compiled with G++ " << __GNUC__ << "." << __GNUC_MINOR__ << std::endl;
    #endif
    #ifdef __clang__
    std::cout << "Compiled with Clang++ " << __clang_major__ << "." << __clang_minor__ << std::endl;
    #endif
    return 0;
}