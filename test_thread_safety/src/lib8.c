#include <stdio.h>

void lib8_function() {
    printf("Library 8 function called\n");
}

int lib8_compute(int x) {
    // Simulate some computation
    int result = x;
    for (int j = 0; j < 1000; j++) {
        result = (result * 8 + j) % 1000000;
    }
    return result;
}
