#include <stdio.h>

void lib2_function() {
    printf("Library 2 function called\n");
}

int lib2_compute(int x) {
    // Simulate some computation
    int result = x;
    for (int j = 0; j < 1000; j++) {
        result = (result * 2 + j) % 1000000;
    }
    return result;
}
