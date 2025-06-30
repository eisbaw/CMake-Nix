#include "math/calculator.h"  // Should pull in config.h transitively
#include <stdlib.h>
#include <stdio.h>

Calculator* calc_create(void) {
    LOG("Creating calculator");
    Calculator* calc = malloc(sizeof(Calculator));
    calc->result = 0.0;
    calc->operation_count = 0;
    return calc;
}

void calc_destroy(Calculator* calc) {
    if (calc) {
        LOG("Destroying calculator");
        free(calc);
    }
}

double calc_add(Calculator* calc, double a, double b) {
    calc->result = a + b;
    calc->operation_count++;
    LOG("Addition performed");
    return calc->result;
}

double calc_multiply(Calculator* calc, double a, double b) {
    calc->result = a * b;
    calc->operation_count++;
    LOG("Multiplication performed");
    return calc->result;
}

void calc_print_stats(Calculator* calc) {
    printf("Calculator stats: %d operations, last result: %.2f\n", 
           calc->operation_count, calc->result);
}