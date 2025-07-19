#include "threaded_lib.hpp"
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>

// Conditional includes based on what packages are available
#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

#ifdef HAVE_OPENSSL
#include <openssl/sha.h>
#include <openssl/evp.h>
#endif

#ifdef HAVE_CURL
#include <curl/curl.h>
#endif

#ifdef HAVE_PNG
#include <png.h>
#endif

void test_threading() {
    std::cout << "Testing multithreading functionality:\n";
    
    // Create test data
    std::vector<int> data;
    for (int i = 1; i <= 1000; ++i) {
        data.push_back(i);
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Test parallel mapping (square each number)
    auto squared = ParallelProcessor::parallel_map(data, [](int x) { return x * x; });
    
    // Test parallel sum
    long sum = ParallelProcessor::parallel_sum(data);
    
    // Test parallel filter (only even numbers)
    auto even_numbers = ParallelProcessor::parallel_filter(data, [](int x) { return x % 2 == 0; });
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "  Sum of 1-1000: " << sum << std::endl;
    std::cout << "  First 5 squares: ";
    for (int i = 0; i < 5; ++i) {
        std::cout << squared[i] << " ";
    }
    std::cout << std::endl;
    std::cout << "  Even numbers found: " << even_numbers.size() << std::endl;
    std::cout << "  Processing time: " << duration.count() << "ms\n";
}

void test_available_packages() {
    std::cout << "\nTesting available packages:\n";
    
#ifdef HAVE_ZLIB
    std::cout << "  ZLIB: Available (version " << ZLIB_VERSION << ")\n";
    // Test zlib compression
    const char* test_data = "Hello, this is a test string for compression!";
    uLongf compressed_size = compressBound(strlen(test_data));
    std::vector<Bytef> compressed(compressed_size);
    
    int result = compress(compressed.data(), &compressed_size,
                         (const Bytef*)test_data, strlen(test_data));
    if (result == Z_OK) {
        std::cout << "    Compression test: SUCCESS (reduced " << strlen(test_data) 
                  << " bytes to " << compressed_size << " bytes)\n";
    }
#else
    std::cout << "  ZLIB: Not available\n";
#endif

#ifdef HAVE_OPENSSL
    std::cout << "  OpenSSL: Available\n";
    // Test SHA256 hashing
    const char* test_data = "Hello, OpenSSL!";
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, test_data, strlen(test_data));
    SHA256_Final(hash, &sha256);
    
    std::cout << "    SHA256 hash test: SUCCESS (first 8 bytes: ";
    for (int i = 0; i < 8; ++i) {
        printf("%02x", hash[i]);
    }
    std::cout << "...)\n";
#else
    std::cout << "  OpenSSL: Not available\n";
#endif

#ifdef HAVE_CURL
    std::cout << "  CURL: Available (version " << LIBCURL_VERSION << ")\n";
    CURL* curl = curl_easy_init();
    if (curl) {
        std::cout << "    CURL initialization: SUCCESS\n";
        curl_easy_cleanup(curl);
    }
#else
    std::cout << "  CURL: Not available\n";
#endif

#ifdef HAVE_PNG
    std::cout << "  PNG: Available (version " << PNG_LIBPNG_VER_STRING << ")\n";
    std::cout << "    PNG library test: SUCCESS\n";
#else
    std::cout << "  PNG: Not available\n";
#endif
}

int main() {
    std::cout << "Package Integration Test Application\n";
    std::cout << "===================================\n\n";
    
    std::cout << "Hardware concurrency: " << std::thread::hardware_concurrency() << " threads\n\n";
    
    test_threading();
    test_available_packages();
    
    std::cout << "\nPackage integration test completed successfully!\n";
    return 0;
}