#include <json/json.h>
#include <iostream>
#include <string>

int main() {
    // Test parsing JSON
    std::string jsonStr = R"({
        "name": "JSON Library Test",
        "version": 1.0,
        "features": ["parsing", "writing", "nested objects"],
        "metadata": {
            "author": "CMake Nix Backend",
            "tested": true,
            "performance": {
                "speed": "fast",
                "memory": "efficient"
            }
        },
        "numbers": [1, 2.5, -3, 4.0]
    })";
    
    std::cout << "Testing JSON library with CMake Nix backend\n";
    std::cout << "==========================================\n\n";
    
    try {
        // Parse JSON
        auto root = json::parse(jsonStr);
        
        // Access values
        std::cout << "Name: " << root->get("name")->asString() << "\n";
        std::cout << "Version: " << root->get("version")->asNumber() << "\n";
        std::cout << "Tested: " << (root->get("metadata")->get("tested")->asBool() ? "true" : "false") << "\n";
        
        // Access nested object
        auto performance = root->get("metadata")->get("performance");
        std::cout << "Performance speed: " << performance->get("speed")->asString() << "\n";
        
        // Access array
        std::cout << "\nFeatures:\n";
        auto features = root->get("features")->asArray();
        for (size_t i = 0; i < features.size(); ++i) {
            std::cout << "  - " << features[i]->asString() << "\n";
        }
        
        // Access number array
        std::cout << "\nNumbers: ";
        auto numbers = root->get("numbers")->asArray();
        for (size_t i = 0; i < numbers.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << numbers[i]->asNumber();
        }
        std::cout << "\n";
        
        // Create new JSON
        json::Object newObj;
        newObj["message"] = std::make_shared<json::Value>("JSON library working!");
        newObj["success"] = std::make_shared<json::Value>(true);
        
        json::Array testResults;
        testResults.push_back(std::make_shared<json::Value>("Parse test passed"));
        testResults.push_back(std::make_shared<json::Value>("Access test passed"));
        testResults.push_back(std::make_shared<json::Value>("Creation test passed"));
        newObj["results"] = std::make_shared<json::Value>(testResults);
        
        auto newRoot = std::make_shared<json::Value>(newObj);
        
        // Stringify
        std::cout << "\nGenerated JSON:\n";
        std::cout << json::stringify(newRoot, true) << "\n";
        
        std::cout << "\n✅ All JSON library tests passed!\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}