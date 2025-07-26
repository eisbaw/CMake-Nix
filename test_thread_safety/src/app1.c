#include <stdio.h>

void lib1_function();
void lib2_function();
void lib3_function();

int main() {
    printf("App1 starting...\n");
    lib1_function();
    lib2_function();
    lib3_function();
    printf("App1 completed!\n");
    return 0;
}
