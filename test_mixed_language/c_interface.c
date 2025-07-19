#include "c_math.h"
#include <stdio.h>

void test_c_functions(void) {
    printf("Testing C functions:\n");
    printf("c_add(10, 5) = %d\n", c_add(10, 5));
    printf("c_multiply(4, 7) = %d\n", c_multiply(4, 7));
    printf("c_divide(20.0, 4.0) = %.2f\n", c_divide(20.0, 4.0));
}