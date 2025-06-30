#include "utils/logger.h"  // Includes config.h and calculator.h transitively
#include <stdio.h>
#include <time.h>

static int log_initialized = 0;

void log_init(void) {
    if (!log_initialized) {
        printf("Logger v%d.%d initialized\n", VERSION_MAJOR, VERSION_MINOR);
        log_initialized = 1;
    }
}

void log_calculation(Calculator* calc, const char* operation) {
    if (!log_initialized) log_init();
    
    time_t now = time(NULL);
    printf("[%ld] %s operation completed. Total ops: %d\n", 
           now, operation, calc->operation_count);
}

void log_error(const char* error_msg) {
    if (!log_initialized) log_init();
    printf("ERROR: %s\n", error_msg);
}