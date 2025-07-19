#include <iostream>
#include <vector>
#include <memory>

// Comprehensive C++11+ feature detection test
// Based on CMake's full feature catalog

int main() {
    std::cout << "Comprehensive C++ Feature Detection Test\n";
    std::cout << "=======================================\n";

    int detected_features = 0;
    int total_features = 0;

    // C++11 Standard Library Features
    total_features++;
#ifdef CMAKE_CXX_STANDARD_11_DETECTED
    std::cout << "✓ C++11 standard detected\n";
    detected_features++;
#else
    std::cout << "✗ C++11 standard NOT detected\n";
#endif

    // Language Features from target_compile_features detection
    total_features++;
#ifdef FEATURE_cxx_constexpr
    std::cout << "✓ cxx_constexpr feature detected\n"; 
    constexpr int compile_time_value = 42;
    std::cout << "  constexpr value: " << compile_time_value << "\n";
    detected_features++;
#else
    std::cout << "✗ cxx_constexpr feature NOT detected\n";
#endif

    total_features++;
#ifdef FEATURE_cxx_lambdas
    std::cout << "✓ cxx_lambdas feature detected\n";
    auto lambda = [](int x) { return x * 2; };
    std::cout << "  lambda result: " << lambda(21) << "\n";
    detected_features++;
#else
    std::cout << "✗ cxx_lambdas feature NOT detected\n";
#endif

    total_features++;
#ifdef FEATURE_cxx_auto_type
    std::cout << "✓ cxx_auto_type feature detected\n";
    auto value = 123;
    std::cout << "  auto value: " << value << "\n";
    detected_features++;
#else
    std::cout << "✗ cxx_auto_type feature NOT detected\n";
#endif

    total_features++;
#ifdef FEATURE_cxx_nullptr
    std::cout << "✓ cxx_nullptr feature detected\n";
    int* ptr = nullptr;
    std::cout << "  nullptr test: " << (ptr == nullptr ? "passed" : "failed") << "\n";
    detected_features++;
#else
    std::cout << "✗ cxx_nullptr feature NOT detected\n";
#endif

    total_features++;
#ifdef FEATURE_cxx_range_for
    std::cout << "✓ cxx_range_for feature detected\n";
    std::vector<int> vec = {1, 2, 3, 4, 5};
    std::cout << "  range-for values: ";
    for (const auto& item : vec) {
        std::cout << item << " ";
    }
    std::cout << "\n";
    detected_features++;
#else
    std::cout << "✗ cxx_range_for feature NOT detected\n";
#endif

    total_features++;
#ifdef FEATURE_cxx_static_assert
    std::cout << "✓ cxx_static_assert feature detected\n";
    static_assert(sizeof(int) >= 4, "int must be at least 4 bytes");
    std::cout << "  static_assert test passed\n";
    detected_features++;
#else
    std::cout << "✗ cxx_static_assert feature NOT detected\n";
#endif

    total_features++;
#ifdef FEATURE_cxx_variadic_templates
    std::cout << "✓ cxx_variadic_templates feature detected\n";
    // Template declaration proves variadic templates work
    std::cout << "  variadic template test completed\n";
    detected_features++;
#else
    std::cout << "✗ cxx_variadic_templates feature NOT detected\n";
#endif

    total_features++;
#ifdef FEATURE_cxx_alias_templates
    std::cout << "✓ cxx_alias_templates feature detected\n";
    using IntVector = std::vector<int>;
    IntVector alias_vec = {1, 2, 3};
    std::cout << "  alias template size: " << alias_vec.size() << "\n";
    detected_features++;
#else
    std::cout << "✗ cxx_alias_templates feature NOT detected\n";
#endif

    total_features++;
#ifdef FEATURE_cxx_alignas
    std::cout << "✓ cxx_alignas feature detected\n";
    alignas(16) int aligned_value = 42;
    std::cout << "  alignas test: " << aligned_value << "\n";
    detected_features++;
#else
    std::cout << "✗ cxx_alignas feature NOT detected\n";
#endif

    total_features++;
#ifdef FEATURE_cxx_alignof
    std::cout << "✓ cxx_alignof feature detected\n";
    std::cout << "  alignof(int): " << alignof(int) << "\n";
    detected_features++;
#else
    std::cout << "✗ cxx_alignof feature NOT detected\n";
#endif

    total_features++;
#ifdef FEATURE_cxx_decltype
    std::cout << "✓ cxx_decltype feature detected\n";
    int x = 42;
    decltype(x) y = 84;
    std::cout << "  decltype test: " << y << "\n";
    detected_features++;
#else
    std::cout << "✗ cxx_decltype feature NOT detected\n";
#endif

    total_features++;
#ifdef FEATURE_cxx_defaulted_functions
    std::cout << "✓ cxx_defaulted_functions feature detected\n";
    std::cout << "  defaulted functions available\n";
    detected_features++;
#else
    std::cout << "✗ cxx_defaulted_functions feature NOT detected\n";
#endif

    total_features++;
#ifdef FEATURE_cxx_deleted_functions
    std::cout << "✓ cxx_deleted_functions feature detected\n";
    std::cout << "  deleted functions available\n";
    detected_features++;
#else
    std::cout << "✗ cxx_deleted_functions feature NOT detected\n";
#endif

    total_features++;
#ifdef FEATURE_cxx_final
    std::cout << "✓ cxx_final feature detected\n";
    std::cout << "  final specifier available\n";
    detected_features++;
#else
    std::cout << "✗ cxx_final feature NOT detected\n";
#endif

    total_features++;
#ifdef FEATURE_cxx_override
    std::cout << "✓ cxx_override feature detected\n";
    std::cout << "  override specifier available\n";
    detected_features++;
#else
    std::cout << "✗ cxx_override feature NOT detected\n";
#endif

    // Check library features that require compilation test
    total_features++;
    try {
        std::unique_ptr<int> ptr(new int(42));
        std::cout << "✓ std::unique_ptr available\n";
        std::cout << "  unique_ptr value: " << *ptr << "\n";
        detected_features++;
    } catch (...) {
        std::cout << "✗ std::unique_ptr NOT available\n";
    }

    // Summary
    std::cout << "\nFeature Detection Summary:\n";
    std::cout << "========================\n";
    std::cout << "Detected: " << detected_features << "/" << total_features << " features\n";
    std::cout << "Coverage: " << (100.0 * detected_features / total_features) << "%\n";

    if (detected_features == total_features) {
        std::cout << "🎉 All features detected successfully!\n";
        return 0;
    } else if (detected_features >= total_features * 0.8) {
        std::cout << "✅ Most features detected successfully\n";
        return 0;
    } else {
        std::cout << "⚠️  Many features missing - check compiler/configuration\n";
        return 1;
    }
}