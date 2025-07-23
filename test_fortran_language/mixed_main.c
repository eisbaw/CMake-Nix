#include <stdio.h>

// Fortran functions with C binding
extern int fortran_add(int a, int b);
extern int fortran_multiply(int a, int b);

int main() {
    printf("Mixed Fortran/C Test\n");
    printf("====================\n");
    
    int a = 10, b = 5;
    
    printf("Using Fortran functions from C:\n");
    printf("fortran_add(%d, %d) = %d\n", a, b, fortran_add(a, b));
    printf("fortran_multiply(%d, %d) = %d\n", a, b, fortran_multiply(a, b));
    
    printf("\nMixed language test successful!\n");
    
    return 0;
}