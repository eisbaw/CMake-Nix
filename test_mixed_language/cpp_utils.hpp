#ifndef CPP_UTILS_HPP
#define CPP_UTILS_HPP

#include <vector>
#include <string>

class MathUtils {
public:
    static std::vector<int> fibonacci(int n);
    static std::string format_result(int value);
    static double power(double base, int exp);
};

// Use C library functions in C++ context
class CCalculator {
public:
    int add(int a, int b);
    int multiply(int a, int b);
    double divide(double a, double b);
};

#endif // CPP_UTILS_HPP