#ifndef GRAPHICS_LIB_HPP
#define GRAPHICS_LIB_HPP

#include "math_lib.hpp"
#include <vector>

namespace graphics {
    struct Color {
        unsigned char r, g, b, a;
        Color(unsigned char r = 0, unsigned char g = 0, unsigned char b = 0, unsigned char a = 255)
            : r(r), g(g), b(b), a(a) {}
    };
    
    class Shape {
    public:
        virtual ~Shape() = default;
        virtual double area() const = 0;
        virtual std::string describe() const = 0;
    };
    
    class Circle : public Shape {
    private:
        math::Vector2D center;
        double radius;
    public:
        Circle(const math::Vector2D& center, double radius);
        double area() const override;
        std::string describe() const override;
    };
    
    class Rectangle : public Shape {
    private:
        math::Vector2D corner1, corner2;
    public:
        Rectangle(const math::Vector2D& c1, const math::Vector2D& c2);
        double area() const override;
        std::string describe() const override;
    };
    
    class Renderer {
    public:
        static void draw_shape(const Shape& shape, const Color& color);
        static std::string color_to_string(const Color& color);
    };
}

#endif // GRAPHICS_LIB_HPP