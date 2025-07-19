#include <stdio.h>

// Declare functions from both libraries
extern int add_numbers(int a, int b);
extern void shared_function();
extern void static_helper();

int main() {
    printf("Testing mixed library linking\n");
    
    static_helper();
    shared_function();
    
    int result = add_numbers(10, 7);
    printf("Mixed result: %d\n", result);
    
    return 0;
}