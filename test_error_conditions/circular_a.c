// Circular dependency test - File A
extern void function_b(void);

void function_a(void) {
    function_b();
}