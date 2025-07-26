#include <stdio.h>

void lib3_function() {
    printf("Library 3 function called\n");
}

int lib3_compute(int x) {
    // Simulate some computation
    int result = x;
    for (int j = 0; j < 1000; j++) {
        result = (result * 3 + j) % 1000000;
    }
    return result;
}
