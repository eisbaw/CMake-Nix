#include <iostream>
#include "generated.h"

int main() {
    #ifdef GENERATED
    std::cout << "Generated header detected!" << std::endl;
    #else
    std::cout << "ERROR: Generated header not found!" << std::endl;
    #endif
    return 0;
}