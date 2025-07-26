#include <stdio.h>

void lib4_function();
void lib5_function();
void lib6_function();

int main() {
    printf("App2 starting...\n");
    lib4_function();
    lib5_function();
    lib6_function();
    printf("App2 completed!\n");
    return 0;
}
