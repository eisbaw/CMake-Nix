#include <stdio.h>

const char* get_version() {
    return "1.2.3";
}

void versioned_function() {
    printf("Versioned library v%s\n", get_version());
}