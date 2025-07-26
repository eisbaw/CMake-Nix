#include <stdio.h>

void lib9_function() {
    printf("Library 9 function called\n");
}

int lib9_compute(int x) {
    // Simulate some computation
    int result = x;
    for (int j = 0; j < 1000; j++) {
        result = (result * 9 + j) % 1000000;
    }
    return result;
}
