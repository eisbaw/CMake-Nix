// Circular dependency test - File B
extern void function_a(void);

void function_b(void) {
    function_a();
}