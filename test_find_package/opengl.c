#include <stdio.h>

// This is a minimal OpenGL test - may not compile if OpenGL headers aren't found
// but serves as a test case for find_package() detection

int main() {
    printf("OpenGL test program\n");
    printf("This would use OpenGL functions if properly linked\n");
    return 0;
}