#include <stdio.h>

void lib6_function() {
    printf("Library 6 function called\n");
}

int lib6_compute(int x) {
    // Simulate some computation
    int result = x;
    for (int j = 0; j < 1000; j++) {
        result = (result * 6 + j) % 1000000;
    }
    return result;
}
