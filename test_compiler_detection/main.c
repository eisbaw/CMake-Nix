#include <stdio.h>

int main() {
    printf("C program compiled with detected compiler\n");
    #ifdef __GNUC__
    printf("Compiled with GCC %d.%d\n", __GNUC__, __GNUC_MINOR__);
    #endif
    #ifdef __clang__
    printf("Compiled with Clang %d.%d\n", __clang_major__, __clang_minor__);
    #endif
    return 0;
}