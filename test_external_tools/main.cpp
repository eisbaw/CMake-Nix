#include <iostream>
#include <fmt/core.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

int main() {
    std::cout << "Testing External Tools with CMake Nix Backend\n";
    std::cout << "============================================\n\n";
    
    // Test FetchContent (fmt library)
    std::string formatted = fmt::format("FetchContent test: {} + {} = {}", 2, 3, 5);
    std::cout << formatted << "\n";
    
    // Test ExternalProject_Add (nlohmann/json)
    json j = {
        {"tool", "ExternalProject_Add"},
        {"status", "working"},
        {"features", {"git", "configure", "build", "install"}}
    };
    
    std::cout << "\nExternalProject test:\n";
    std::cout << j.dump(2) << "\n";
    
    std::cout << "\n✅ Both ExternalProject_Add and FetchContent are working!\n";
    
    return 0;
}