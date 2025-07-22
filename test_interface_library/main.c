#include <stdio.h>

int main() {
#ifdef USE_COMMON_FLAGS
    printf("Interface library definitions are working!\n");
#else
    printf("ERROR: Interface library definitions not applied!\n");
#endif
    return 0;
}