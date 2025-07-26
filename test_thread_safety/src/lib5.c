#include <stdio.h>

void lib5_function() {
    printf("Library 5 function called\n");
}

int lib5_compute(int x) {
    // Simulate some computation
    int result = x;
    for (int j = 0; j < 1000; j++) {
        result = (result * 5 + j) % 1000000;
    }
    return result;
}
