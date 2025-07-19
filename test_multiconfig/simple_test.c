#include <stdio.h>

int main() {
    printf("Simple Multi-Config Test\n");
    
#ifdef NDEBUG
    printf("NDEBUG is defined (Release/RelWithDebInfo/MinSizeRel)\n");
#else
    printf("NDEBUG is NOT defined (Debug)\n");
#endif

    return 0;
}