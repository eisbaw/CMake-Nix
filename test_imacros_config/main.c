#include <stdio.h>

// Note: We don't need to include generated_config.h explicitly
// because it's included via -imacros flag

int main() {
    printf("Testing -imacros flag with configuration-time generated files\n");
    printf("=============================================================\n\n");
    
    // These macros are defined in generated_config.h which is included via -imacros
    printf("Project: %s\n", PROJECT_NAME);
    printf("Test value: %d\n", TEST_VALUE);
    printf("Greeting: %s\n", GREETING);
    printf("Double of 21: %d\n", DOUBLE(21));
    
    // Verify the macros work correctly
    if (TEST_VALUE == 42 && DOUBLE(21) == 42) {
        printf("\n✅ Success! -imacros flag correctly includes generated config file\n");
        return 0;
    } else {
        printf("\n❌ Error: Macros not working as expected\n");
        return 1;
    }
}