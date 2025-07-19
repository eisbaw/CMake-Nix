#include <stdio.h>

// Declare the function from the generated file
extern int get_generated_value();

int main() {
    printf("Main function\n");
    printf("Generated value: %d\n", get_generated_value());
    return 0;
}