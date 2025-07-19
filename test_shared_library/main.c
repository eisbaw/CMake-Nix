#include <stdio.h>

// Declare shared library functions
extern int add_numbers(int a, int b);
extern void shared_function();

int main() {
    printf("Testing shared library\n");
    shared_function();
    
    int result = add_numbers(5, 3);
    printf("Result: %d\n", result);
    
    return 0;
}