#include "math_lib.hpp"
#include <cmath>
#include <sstream>

namespace math {
    Vector2D VectorMath::add(const Vector2D& a, const Vector2D& b) {
        base::Logger::debug("Adding 2D vectors");
        return Vector2D(a.x + b.x, a.y + b.y);
    }
    
    Vector3D VectorMath::add(const Vector3D& a, const Vector3D& b) {
        base::Logger::debug("Adding 3D vectors");
        return Vector3D(a.x + b.x, a.y + b.y, a.z + b.z);
    }
    
    double VectorMath::dot(const Vector2D& a, const Vector2D& b) {
        return a.x * b.x + a.y * b.y;
    }
    
    double VectorMath::dot(const Vector3D& a, const Vector3D& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
    
    double VectorMath::magnitude(const Vector2D& v) {
        return std::sqrt(v.x * v.x + v.y * v.y);
    }
    
    double VectorMath::magnitude(const Vector3D& v) {
        return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    }
    
    double Calculator::factorial(int n) {
        if (n <= 1) return 1;
        double result = 1;
        for (int i = 2; i <= n; ++i) {
            result *= i;
        }
        return result;
    }
    
    double Calculator::power(double base, int exp) {
        return std::pow(base, exp);
    }
    
    bool Calculator::is_prime(int n) {
        if (n < 2) return false;
        for (int i = 2; i * i <= n; ++i) {
            if (n % i == 0) return false;
        }
        return true;
    }
}