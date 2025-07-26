#include <stdio.h>

void function_b(void);

void function_a(void) {
    printf("Function A calling B\n");
    function_b();
}