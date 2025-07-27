#include <stdio.h>
#include "external_config.h"  // This is a config-time generated header

int external_compute(int value) {
    // This external source file depends on a configuration-time generated header
    printf("External module from %s says: %s\n", EXTERNAL_PROJECT, EXTERNAL_MESSAGE);
    
    // Use the configured multiplier
    return value * EXTERNAL_MULTIPLIER;
}