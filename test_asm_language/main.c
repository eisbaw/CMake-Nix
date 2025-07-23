#include <stdio.h>

// External function defined in assembly
extern void hello_asm(void);

int main() {
    printf("Testing ASM language support\n");
    hello_asm();
    printf("ASM test completed successfully!\n");
    return 0;
}