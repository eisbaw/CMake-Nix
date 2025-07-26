#include <stdio.h>

void function_a(void);

void function_b(void) {
    printf("Function B calling A\n");
    // Don't actually call to avoid infinite recursion
    // function_a();
}