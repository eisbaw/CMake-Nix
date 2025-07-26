#include <stdio.h>
#include "nonexistent_header.h"  // This header doesn't exist

int main() {
    printf("This file includes a missing header\n");
    return 0;
}