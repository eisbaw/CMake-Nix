#include <iostream>

extern "C" {
    const char* get_shared_message();
}

int main() {
    std::cout << get_shared_message() << std::endl;
    return 0;
}