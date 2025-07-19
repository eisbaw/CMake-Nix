#include "graphics_lib.hpp"
#include <sstream>
#include <cmath>

namespace graphics {
    Circle::Circle(const math::Vector2D& center, double radius) 
        : center(center), radius(radius) {
        base::Logger::debug("Created circle");
    }
    
    double Circle::area() const {
        return M_PI * radius * radius;
    }
    
    std::string Circle::describe() const {
        std::ostringstream oss;
        oss << "Circle at (" << center.x << ", " << center.y << ") with radius " << radius;
        return oss.str();
    }
    
    Rectangle::Rectangle(const math::Vector2D& c1, const math::Vector2D& c2)
        : corner1(c1), corner2(c2) {
        base::Logger::debug("Created rectangle");
    }
    
    double Rectangle::area() const {
        double width = std::abs(corner2.x - corner1.x);
        double height = std::abs(corner2.y - corner1.y);
        return width * height;
    }
    
    std::string Rectangle::describe() const {
        std::ostringstream oss;
        oss << "Rectangle from (" << corner1.x << ", " << corner1.y 
            << ") to (" << corner2.x << ", " << corner2.y << ")";
        return oss.str();
    }
    
    void Renderer::draw_shape(const Shape& shape, const Color& color) {
        std::ostringstream oss;
        oss << "Drawing " << shape.describe() << " with color " << color_to_string(color);
        base::Logger::log(oss.str());
    }
    
    std::string Renderer::color_to_string(const Color& color) {
        std::ostringstream oss;
        oss << "RGBA(" << (int)color.r << ", " << (int)color.g 
            << ", " << (int)color.b << ", " << (int)color.a << ")";
        return oss.str();
    }
}