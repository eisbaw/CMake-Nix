// Simple C++ test without standard library to avoid Nix header issues
#include "mylib.h"

// C linkage for printf
extern "C" int printf(const char*, ...);

// Forward declarations for conditional sources
const char* get_utils_info();

int main() {
    printf("Generator Expressions Test\n");
    printf("%s\n", get_build_info());
    printf("%s\n", get_utils_info());
    return 0;
}