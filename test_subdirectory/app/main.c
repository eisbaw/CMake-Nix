#include "math.h"
#include <stdio.h>

int main() {
    printf("Calculator Application\n");
    printf("======================\n");
    
    int result1 = add(10, 5);
    printf("Result: %d\n\n", result1);
    
    int result2 = multiply(7, 3);
    printf("Result: %d\n", result2);
    
    return 0;
}