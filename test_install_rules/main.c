#include <stdio.h>

extern void lib_function(void);
extern void static_function(void);

int main(void) {
    printf("Install Rules Test Application\n");
    printf("==============================\n");
    
    lib_function();
    static_function();
    
    printf("Application completed successfully!\n");
    return 0;
}