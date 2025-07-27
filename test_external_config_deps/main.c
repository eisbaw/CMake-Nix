#include <stdio.h>
#include "config.h"

// Declare external function
int external_compute(int value);

int main() {
    printf("Project: %s v%s\n", PROJECT_NAME, PROJECT_VERSION);
    
    int input = EXTERNAL_VALUE;
    int result = external_compute(input);
    
    printf("External computation: %d -> %d\n", input, result);
    
    if (result == input * 2) {
        printf("✅ Test passed: External source correctly used config-time generated headers\n");
        return 0;
    } else {
        printf("❌ Test failed: Expected %d, got %d\n", input * 2, result);
        return 1;
    }
}