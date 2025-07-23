#include <iostream>

// Declare functions from other files
void function1();
void function2();
void function3();

int main() {
    std::cout << "Unity Build Test Application" << std::endl;
    std::cout << "=============================" << std::endl;
    
    function1();
    function2();
    function3();
    
    std::cout << "\nAll functions executed successfully!" << std::endl;
    return 0;
}