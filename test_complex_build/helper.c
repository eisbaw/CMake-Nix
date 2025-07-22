#include "helpers.h"
#include <stdio.h>

void print_version() {
    printf("Version %d.%d\n", VERSION_MAJOR, VERSION_MINOR);
#ifdef FEATURE_X_ENABLED
    printf("Feature X is enabled\n");
#endif
}

int helper_function(int x) {
    printf("Helper function called (C99) with value: %d\n", x);
    return x * 2;
}