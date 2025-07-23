#include <stdio.h>
#include "helper.h"

int main() {
    printf("Testing explicit sources feature\n");
    printf("Helper result: %d\n", helper_function(5, 10));
    printf("Square of 7: %d\n", square(7));
    return 0;
}