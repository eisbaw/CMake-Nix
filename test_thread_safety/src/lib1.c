#include <stdio.h>

void lib1_function() {
    printf("Library 1 function called\n");
}

int lib1_compute(int x) {
    // Simulate some computation
    int result = x;
    for (int j = 0; j < 1000; j++) {
        result = (result * 1 + j) % 1000000;
    }
    return result;
}
