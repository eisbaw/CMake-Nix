#include "config.h"
#include <stdio.h>

int main() {
    printf("Preprocessor Test Application\n");
    printf("============================\n\n");
    
    // App information
    #ifdef APP_NAME
    printf("Application: %s\n", "PreprocessorTest");
    #endif
    
    #ifdef COMPILE_DATE
    printf("Compiled: %s\n", "DATE");
    #endif
    
    printf("\n");
    
    // Configuration from library
    print_config();
    printf("\nVersion Number: %d\n", get_version_number());
    
    // Debug vs Release build
    printf("\nBuild Configuration:\n");
    #ifdef DEBUG_BUILD
    printf("Build Type: DEBUG\n");
    #ifdef ENABLE_ASSERTS
    printf("Assertions: ENABLED\n");
    #endif
    #else
    printf("Build Type: RELEASE\n");
    #ifdef OPTIMIZE_BUILD
    printf("Optimizations: ENABLED\n");
    #endif
    #ifdef DISABLE_LOGGING
    printf("Logging: DISABLED for performance\n");
    #endif
    #endif
    
    // Test logging macros
    printf("\nTesting conditional macros:\n");
    LOG("This message shows if logging is enabled");
    DEBUG_PRINT("Debug level is %d", DEBUG_LEVEL);
    
    printf("\nPreprocessor test completed!\n");
    return 0;
}