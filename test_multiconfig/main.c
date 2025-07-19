#include <stdio.h>

int main() {
    printf("Multi-Config Test Application\n");
    printf("============================\n");
    
#ifdef DEBUG_BUILD
    printf("Configuration: Debug\n");
    printf("Optimization: -g -O0\n");
#elif defined(RELEASE_BUILD)
    printf("Configuration: Release\n");
    printf("Optimization: -O3 -DNDEBUG\n");
#elif defined(RELWITHDEBINFO_BUILD)
    printf("Configuration: RelWithDebInfo\n");
    printf("Optimization: -g -O2 -DNDEBUG\n");
#elif defined(MINSIZEREL_BUILD)
    printf("Configuration: MinSizeRel\n");
    printf("Optimization: -Os -DNDEBUG\n");
#else
    printf("Configuration: Unknown/Default\n");
    printf("Optimization: Default\n");
#endif

    printf("Application completed successfully!\n");
    return 0;
}