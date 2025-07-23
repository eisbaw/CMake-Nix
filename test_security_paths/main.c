#include <stdio.h>

int main() {
    printf("Security path test executable\n");
    printf("This tests that the Nix generator properly handles:\n");
    printf("- Paths with spaces\n");
    printf("- Special characters like quotes, dollars, backticks\n");
    printf("- Long file names\n");
    printf("Test passed if this compiles and runs!\n");
    return 0;
}