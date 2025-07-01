#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <zlib.h>

int main() {
    printf("ZLib Compression Demo - Pure Nix Library Approach\n");
    printf("=================================================\n");
    
    // Show zlib version
    printf("ZLib version: %s\n", zlibVersion());
    
    // Test data to compress
    const char* original = "Hello from CMake Nix Generator! "
                          "This is a test of zlib compression functionality. "
                          "The CMake Nix generator automatically detected the zlib dependency, "
                          "generated a pkg_z.nix file, and linked it properly in the Nix derivation. "
                          "This demonstrates the pure Nix approach for external library management!";
    
    uLong original_len = strlen(original) + 1;
    printf("\nOriginal text (%lu bytes):\n%s\n", original_len, original);
    
    // Compress the data
    uLong compressed_len = compressBound(original_len);
    Bytef* compressed = (Bytef*)malloc(compressed_len);
    
    int result = compress(compressed, &compressed_len, (const Bytef*)original, original_len);
    if (result != Z_OK) {
        printf("Compression failed with error: %d\n", result);
        free(compressed);
        return 1;
    }
    
    printf("\nCompressed data (%lu bytes) - Compression ratio: %.1f%%\n", 
           compressed_len, 100.0 * compressed_len / original_len);
    
    // Decompress the data
    uLong decompressed_len = original_len;
    Bytef* decompressed = (Bytef*)malloc(decompressed_len);
    
    result = uncompress(decompressed, &decompressed_len, compressed, compressed_len);
    if (result != Z_OK) {
        printf("Decompression failed with error: %d\n", result);
        free(compressed);
        free(decompressed);
        return 1;
    }
    
    printf("\nDecompressed text (%lu bytes):\n%s\n", decompressed_len, decompressed);
    
    // Verify the data matches
    if (strcmp(original, (const char*)decompressed) == 0) {
        printf("\n✅ Success! Compression and decompression worked correctly.\n");
        printf("✅ ZLib external library integration via pure Nix approach is working!\n");
    } else {
        printf("\n❌ Error: Decompressed data doesn't match original!\n");
        free(compressed);
        free(decompressed);
        return 1;
    }
    
    free(compressed);
    free(decompressed);
    
    printf("\nPure Nix library test completed successfully!\n");
    return 0;
}