#include "helper.h"
#include "internal.h"

int helper_function(int a, int b) {
    return internal_add(a, b);
}

int square(int x) {
    return internal_multiply(x, x);
}