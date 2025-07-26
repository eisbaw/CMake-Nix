#include <stdio.h>

void lib4_function() {
    printf("Library 4 function called\n");
}

int lib4_compute(int x) {
    // Simulate some computation
    int result = x;
    for (int j = 0; j < 1000; j++) {
        result = (result * 4 + j) % 1000000;
    }
    return result;
}
