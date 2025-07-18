#include "math/calculator.h"
#include "utils/logger.h"  // Complex transitive dependencies
#include <stdio.h>

int main() {
    log_init();
    
    Calculator* calc = calc_create();
    if (!calc) {
        log_error("Failed to create calculator");
        return 1;
    }
    
    double result1 = calc_add(calc, 15.5, 24.3);
    log_calculation(calc, "add");
    printf("15.5 + 24.3 = %.2f\n", result1);
    
    double result2 = calc_multiply(calc, result1, 2.0);
    log_calculation(calc, "multiply");
    printf("%.2f * 2.0 = %.2f\n", result1, result2);
    
    calc_print_stats(calc);
    calc_destroy(calc);
    
    return 0;
}