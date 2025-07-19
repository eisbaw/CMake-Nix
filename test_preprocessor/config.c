#include "config.h"
#include <stdio.h>

void print_config(void) {
    printf("Configuration Information:\n");
    printf("========================\n");
    printf("Version: %d.%d.%d\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
    
    #ifdef BUILD_NUMBER
    printf("Build Number: %d\n", BUILD_NUMBER);
    #endif
    
    printf("Config String: %s\n", "production");
    printf("Debug Level: %d\n", DEBUG_LEVEL);
    printf("Max Connections: %d\n", MAX_CONNECTIONS);
    printf("Default Timeout: %d\n", DEFAULT_TIMEOUT);
    
    printf("Features:\n");
    #if FEATURE_LOGGING
    printf("  - Logging: ENABLED\n");
    #else
    printf("  - Logging: DISABLED\n");
    #endif
    
    #if FEATURE_NETWORKING
    printf("  - Networking: ENABLED\n");
    #else
    printf("  - Networking: DISABLED\n");
    #endif
}

int get_version_number(void) {
    return VERSION_MAJOR * 10000 + VERSION_MINOR * 100 + VERSION_PATCH;
}