#include "graphics_lib.hpp"
#include "math_lib.hpp"
#include "base_utils.hpp"
#include <iostream>

int main() {
    std::cout << "Direct Dependencies Test Application\n";
    std::cout << "===================================\n\n";
    
    // Test base utilities directly
    base::Logger::log("Testing direct library access");
    
    // Test string utilities
    std::string test_string = "  Hello, World!  ";
    std::string trimmed = base::StringUtils::trim(test_string);
    std::cout << "Original: '" << test_string << "'" << std::endl;
    std::cout << "Trimmed: '" << trimmed << "'" << std::endl;
    
    std::vector<std::string> parts = base::StringUtils::split("apple,banana,cherry", ',');
    std::cout << "Split result: ";
    for (const auto& part : parts) {
        std::cout << "[" << part << "] ";
    }
    std::cout << std::endl;
    
    // Test math operations
    std::cout << "\nTesting math operations:\n";
    math::Vector2D v1(3, 4);
    math::Vector2D v2(1, 2);
    math::Vector2D sum = math::VectorMath::add(v1, v2);
    std::cout << "Vector sum: (" << sum.x << ", " << sum.y << ")" << std::endl;
    
    double magnitude = math::VectorMath::magnitude(v1);
    std::cout << "Vector magnitude: " << magnitude << std::endl;
    
    // Test graphics
    std::cout << "\nTesting graphics:\n";
    graphics::Circle circle(math::Vector2D(0, 0), 5);
    graphics::Rectangle rect(math::Vector2D(0, 0), math::Vector2D(10, 8));
    
    std::cout << circle.describe() << " - Area: " << circle.area() << std::endl;
    std::cout << rect.describe() << " - Area: " << rect.area() << std::endl;
    
    // Render shapes
    graphics::Renderer::draw_shape(circle, graphics::Color(255, 100, 50));
    graphics::Renderer::draw_shape(rect, graphics::Color(100, 255, 150));
    
    std::cout << "\nDirect dependencies test completed successfully!\n";
    return 0;
}