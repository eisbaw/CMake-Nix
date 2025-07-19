#include "c_math.h"

int c_add(int a, int b) {
    return a + b;
}

int c_multiply(int a, int b) {
    return a * b;
}

double c_divide(double a, double b) {
    if (b == 0.0) return 0.0;
    return a / b;
}