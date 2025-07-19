#include <stdio.h>

int add_numbers(int a, int b) {
    printf("Shared library function: %d + %d\n", a, b);
    return a + b;
}

void shared_function() {
    printf("Hello from shared library!\n");
}