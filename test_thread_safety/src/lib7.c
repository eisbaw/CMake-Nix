#include <stdio.h>

void lib7_function() {
    printf("Library 7 function called\n");
}

int lib7_compute(int x) {
    // Simulate some computation
    int result = x;
    for (int j = 0; j < 1000; j++) {
        result = (result * 7 + j) % 1000000;
    }
    return result;
}
