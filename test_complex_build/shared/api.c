#include "shared_api.h"
#include <stdio.h>

static int initialized = 0;

void shared_init() {
    if (!initialized) {
        printf("Initializing shared library...\n");
        initialized = 1;
    }
}

int shared_compute(int a, int b) {
    if (!initialized) {
        printf("Warning: shared library not initialized!\n");
    }
    printf("Shared compute: %d * %d = %d\n", a, b, a * b);
    return a * b;
}