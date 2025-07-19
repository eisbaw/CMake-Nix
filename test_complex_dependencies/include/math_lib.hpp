#ifndef MATH_LIB_HPP
#define MATH_LIB_HPP

#include "base_utils.hpp"

namespace math {
    struct Vector2D {
        double x, y;
        Vector2D(double x = 0, double y = 0) : x(x), y(y) {}
    };
    
    struct Vector3D {
        double x, y, z;
        Vector3D(double x = 0, double y = 0, double z = 0) : x(x), y(y), z(z) {}
    };
    
    class VectorMath {
    public:
        static Vector2D add(const Vector2D& a, const Vector2D& b);
        static Vector3D add(const Vector3D& a, const Vector3D& b);
        static double dot(const Vector2D& a, const Vector2D& b);
        static double dot(const Vector3D& a, const Vector3D& b);
        static double magnitude(const Vector2D& v);
        static double magnitude(const Vector3D& v);
    };
    
    class Calculator {
    public:
        static double factorial(int n);
        static double power(double base, int exp);
        static bool is_prime(int n);
    };
}

#endif // MATH_LIB_HPP