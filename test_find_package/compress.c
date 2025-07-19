#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

int main() {
    printf("Testing zlib compression\n");
    
    const char* input = "Hello, this is a test string for compression!";
    int input_len = strlen(input);
    
    // Compress
    uLongf compressed_len = compressBound(input_len);
    unsigned char* compressed = malloc(compressed_len);
    
    if (compress(compressed, &compressed_len, (const Bytef*)input, input_len) != Z_OK) {
        printf("Compression failed\n");
        free(compressed);
        return 1;
    }
    
    printf("Original: %d bytes\n", input_len);
    printf("Compressed: %ld bytes\n", compressed_len);
    printf("Compression ratio: %.2f%%\n", (1.0 - (double)compressed_len / input_len) * 100);
    
    // Decompress
    uLongf decompressed_len = input_len;
    unsigned char* decompressed = malloc(decompressed_len);
    
    if (uncompress(decompressed, &decompressed_len, compressed, compressed_len) != Z_OK) {
        printf("Decompression failed\n");
        free(compressed);
        free(decompressed);
        return 1;
    }
    
    printf("Decompressed: %.*s\n", (int)decompressed_len, decompressed);
    printf("Match: %s\n", (memcmp(input, decompressed, input_len) == 0) ? "YES" : "NO");
    
    free(compressed);
    free(decompressed);
    return 0;
}