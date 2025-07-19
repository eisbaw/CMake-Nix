#include "engine_lib.hpp"
#include <iostream>
#include <memory>

int main() {
    std::cout << "Complex Dependencies Test Application\n";
    std::cout << "====================================\n\n";
    
    // Create an engine (uses all libraries transitively)
    engine::Engine gameEngine;
    
    // Create some shapes using graphics and math libraries
    auto circle = std::make_unique<graphics::Circle>(math::Vector2D(5, 5), 3.0);
    auto rectangle = std::make_unique<graphics::Rectangle>(
        math::Vector2D(0, 0), math::Vector2D(4, 3));
    
    // Create game objects using the shapes
    auto obj1 = std::make_unique<engine::GameObject>(
        math::Vector3D(1, 2, 3), std::move(circle), graphics::Color(255, 0, 0, 255));
    auto obj2 = std::make_unique<engine::GameObject>(
        math::Vector3D(4, 5, 6), std::move(rectangle), graphics::Color(0, 255, 0, 255));
    
    // Set velocities
    obj1->set_velocity(math::Vector3D(1, 0, 0));
    obj2->set_velocity(math::Vector3D(0, 1, 0));
    
    // Add to engine
    gameEngine.add_object(std::move(obj1));
    gameEngine.add_object(std::move(obj2));
    
    std::cout << "Initial state:\n";
    std::cout << gameEngine.get_engine_stats() << std::endl;
    
    // Simulate a few frames
    std::cout << "\nSimulating 3 frames:\n";
    for (int i = 0; i < 3; ++i) {
        std::cout << "\nFrame " << (i + 1) << ":\n";
        gameEngine.simulate_frame(0.1);
    }
    
    std::cout << "\nFinal state:\n";
    std::cout << gameEngine.get_engine_stats() << std::endl;
    
    // Test math library directly
    std::cout << "\nTesting math operations:\n";
    std::cout << "5! = " << math::Calculator::factorial(5) << std::endl;
    std::cout << "2^8 = " << math::Calculator::power(2, 8) << std::endl;
    std::cout << "Is 17 prime? " << (math::Calculator::is_prime(17) ? "Yes" : "No") << std::endl;
    
    // Test base utilities
    std::cout << "\nTesting string utilities:\n";
    std::vector<std::string> words = {"Complex", "Dependencies", "Test"};
    std::string joined = base::StringUtils::join(words, " -> ");
    std::cout << "Joined words: " << joined << std::endl;
    
    std::cout << "\nComplex dependencies test completed successfully!\n";
    return 0;
}