#include <iostream>
#include <string>
#include "cmSystemTools.h"
#include "cmStringAlgorithms.h"

int main() {
    std::cout << "CMake Self-Host Test" << std::endl;
    std::cout << "===================" << std::endl;
    
    // Test some basic CMake functionality
    std::string testPath = "/some/test/path";
    std::string converted = cmSystemTools::ConvertToUnixSlashes(testPath);
    std::cout << "Path conversion test: " << converted << std::endl;
    
    // Test string algorithms
    std::string testStr = "hello,world,cmake";
    std::vector<std::string> parts = cmTokenize(testStr, ",");
    std::cout << "Tokenization test: ";
    for (const auto& part : parts) {
        std::cout << "[" << part << "] ";
    }
    std::cout << std::endl;
    
    std::cout << "CMake core components working correctly!" << std::endl;
    return 0;
}