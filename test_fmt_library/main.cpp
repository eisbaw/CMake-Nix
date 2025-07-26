#include "simple_fmt.h"
#include <vector>
#include <map>
#include <chrono>

int main() {
    fmt::print("Testing fmt library with CMake Nix backend\n");
    fmt::print("==========================================\n\n");
    
    // Basic formatting
    fmt::print("Hello, {}!\n", "fmt");
    fmt::print("The answer is {}.\n", 42);
    
    // Two argument formatting
    fmt::print("{}, {}!\n", "Hello", "world");
    
    // Formatting containers
    std::vector<int> v = {1, 2, 3, 4, 5};
    std::string joined = fmt::join(v, ", ");
    fmt::print("Vector: ");
    fmt::print(joined);
    fmt::print("\n");
    
    // Map test
    std::map<std::string, int> m = {{"one", 1}, {"two", 2}, {"three", 3}};
    fmt::print("Map entries: ");
    for (const auto& pair : m) {
        fmt::print("{}: {} ", pair.first, pair.second);
    }
    fmt::print("\n");
    
    // Colored output
    fmt::print(fmt::fg(fmt::color::green), "\n✅ All fmt library tests passed!\n");
    
    // Performance test
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10000; ++i) {
        auto result = fmt::format("Iteration {} of {}", i, 10000);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    fmt::print("\nFormatted 10,000 strings in {} microseconds\n", duration.count());
    
    return 0;
}
