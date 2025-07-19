#include "cpp_utils.hpp"
#include "c_math.h"
#include <sstream>
#include <cmath>

std::vector<int> MathUtils::fibonacci(int n) {
    std::vector<int> result;
    if (n <= 0) return result;
    
    if (n >= 1) result.push_back(0);
    if (n >= 2) result.push_back(1);
    
    for (int i = 2; i < n; ++i) {
        result.push_back(result[i-1] + result[i-2]);
    }
    return result;
}

std::string MathUtils::format_result(int value) {
    std::ostringstream oss;
    oss << "Result: " << value;
    return oss.str();
}

double MathUtils::power(double base, int exp) {
    return std::pow(base, exp);
}

int CCalculator::add(int a, int b) {
    return c_add(a, b);
}

int CCalculator::multiply(int a, int b) {
    return c_multiply(a, b);
}

double CCalculator::divide(double a, double b) {
    return c_divide(a, b);
}