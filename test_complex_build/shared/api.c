#include "api.h"
#include <stdio.h>

void shared_api_function(void) {
    printf("Shared library API function\n");
    shared_impl_function();
}