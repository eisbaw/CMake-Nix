#include <string>

std::string getOptimizationLevel() {
    // This function's optimization will vary by configuration
    volatile int sum = 0;
    
    // In Debug: This loop won't be optimized out
    // In Release/RelWithDebInfo: May be partially optimized
    // In MinSizeRel: Optimized for size
    for (int i = 0; i < 100; ++i) {
        sum += i;
    }
    
#ifdef DEBUG_MODE
    return "Debug build - no optimization";
#elif defined(RELEASE_MODE)
    return "Release build - full optimization";
#elif defined(RELWITHDEBINFO_MODE)
    return "RelWithDebInfo - optimized with debug info";
#elif defined(MINSIZEREL_MODE)
    return "MinSizeRel - size optimization";
#else
    return "Unknown configuration";
#endif
}