#include <stdio.h>

void lib10_function() {
    printf("Library 10 function called\n");
}

int lib10_compute(int x) {
    // Simulate some computation
    int result = x;
    for (int j = 0; j < 1000; j++) {
        result = (result * 10 + j) % 1000000;
    }
    return result;
}
