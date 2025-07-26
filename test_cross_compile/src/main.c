#include <stdio.h>

int main() {
    printf("Cross-compilation test program\n");
    printf("==============================\n\n");
    
#ifdef CROSS_COMPILING
    printf("Built with CROSS-COMPILATION\n");
    #ifdef TARGET_ARCH
    printf("Target architecture: %s\n", TARGET_ARCH);
    #endif
#else
    printf("Built as NATIVE build\n");
#endif

    printf("\nCompiler info:\n");
#ifdef __GNUC__
    printf("  GCC version: %d.%d.%d\n", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#endif
#ifdef __clang__
    printf("  Clang version: %d.%d.%d\n", __clang_major__, __clang_minor__, __clang_patchlevel__);
#endif

    printf("\nArchitecture info:\n");
#ifdef __x86_64__
    printf("  x86_64 detected\n");
#endif
#ifdef __i386__
    printf("  i386 detected\n");
#endif
#ifdef __arm__
    printf("  ARM detected\n");
#endif
#ifdef __aarch64__
    printf("  ARM64/AArch64 detected\n");
#endif

    printf("\nSizeof info:\n");
    printf("  sizeof(void*): %zu\n", sizeof(void*));
    printf("  sizeof(int): %zu\n", sizeof(int));
    printf("  sizeof(long): %zu\n", sizeof(long));
    
    return 0;
}