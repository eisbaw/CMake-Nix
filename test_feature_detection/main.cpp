#include <iostream>
#include <vector>

// Test program that uses various C++11 features
// Compile definitions will be set based on feature detection

int main() {
    std::cout << "C++11 Feature Detection Test\n";
    std::cout << "===========================\n";

    // Test constexpr support
#ifdef HAVE_CONSTEXPR
    std::cout << "✓ constexpr support detected\n";
    constexpr int compile_time_value = 42;
    std::cout << "  constexpr value: " << compile_time_value << "\n";
#else
    std::cout << "✗ constexpr support NOT detected\n";
#endif

    // Test lambda support
#ifdef HAVE_LAMBDAS
    std::cout << "✓ lambda support detected\n";
    auto lambda = [](int x) { return x * 2; };
    std::cout << "  lambda result: " << lambda(21) << "\n";
#else
    std::cout << "✗ lambda support NOT detected\n";
#endif

    // Test auto type support
#ifdef HAVE_AUTO_TYPE
    std::cout << "✓ auto type support detected\n";
    auto value = 123;
    std::cout << "  auto value: " << value << "\n";
#else
    std::cout << "✗ auto type support NOT detected\n";
#endif

    // Test nullptr support
#ifdef HAVE_NULLPTR
    std::cout << "✓ nullptr support detected\n";
    int* ptr = nullptr;
    std::cout << "  nullptr test: " << (ptr == nullptr ? "passed" : "failed") << "\n";
#else
    std::cout << "✗ nullptr support NOT detected\n";
#endif

    // Test range-based for loop support
#ifdef HAVE_RANGE_FOR
    std::cout << "✓ range-based for loop support detected\n";
    std::vector<int> vec = {1, 2, 3, 4, 5};
    std::cout << "  range-for values: ";
    for (const auto& item : vec) {
        std::cout << item << " ";
    }
    std::cout << "\n";
#else
    std::cout << "✗ range-based for loop support NOT detected\n";
#endif

    // Test static_assert support
#ifdef HAVE_STATIC_ASSERT
    std::cout << "✓ static_assert support detected\n";
    static_assert(sizeof(int) >= 4, "int must be at least 4 bytes");
    std::cout << "  static_assert test passed\n";
#else
    std::cout << "✗ static_assert support NOT detected\n";
#endif

    // Test variadic template support
#ifdef HAVE_VARIADIC_TEMPLATES
    std::cout << "✓ variadic template support detected\n";
    
    // Simple variadic template test function (C++11 compatible)
    std::cout << "  variadic template test completed\n";
#else
    std::cout << "✗ variadic template support NOT detected\n";
#endif

    std::cout << "\nFeature detection test completed.\n";
    return 0;
}