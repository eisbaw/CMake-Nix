#include <iostream>
#include <memory>
#include <string>
#include <vector>

// Forward declarations to avoid including all CMake headers
namespace cm {
  class optional { 
  public:
    optional() = default;
    optional(const std::string& s) : value(s), has_value_(true) {}
    bool has_value() const { return has_value_; }
    const std::string& operator*() const { return value; }
  private:
    std::string value;
    bool has_value_ = false;
  };
}

class cmake;
class cmState;
class cmGlobalGenerator;

// Mock cmake class for testing
class cmake {
public:
  cmState* GetState() { return nullptr; }
  std::string GetHomeDirectory() { return "/test"; }
};

// Simplified version of our multi-config generator
class TestNixMultiGenerator {
public:
  TestNixMultiGenerator(cmake* cm) : CMakeInstance(cm) {}
  
  std::vector<std::string> GetConfigurationTypes() const {
    std::vector<std::string> configs;
    configs.push_back("Debug");
    configs.push_back("Release");
    configs.push_back("RelWithDebInfo");
    configs.push_back("MinSizeRel");
    return configs;
  }
  
  std::string GetDefaultConfiguration() const {
    std::vector<std::string> configs = this->GetConfigurationTypes();
    return configs.empty() ? "Release" : configs[0];
  }
  
  std::string GetDerivationNameForConfig(
    const std::string& targetName,
    const std::string& sourceFile,
    const std::string& config) const
  {
    std::string configLower = config;
    // Simple lowercase conversion
    for (char& c : configLower) {
      if (c >= 'A' && c <= 'Z') {
        c = c - 'A' + 'a';
      }
    }
    
    if (sourceFile.empty()) {
      return "link_" + targetName + "_" + configLower;
    } else {
      // For object files
      std::string baseName = targetName + "_" + sourceFile;
      // Replace path separators and dots
      for (char& c : baseName) {
        if (c == '/' || c == '.' || c == '-') {
          c = '_';
        }
      }
      return baseName + "_" + configLower + "_o";
    }
  }
  
  void TestNaming() {
    std::cout << "Testing multi-config derivation naming..." << std::endl;
    
    // Test configurations
    std::vector<std::string> configs = GetConfigurationTypes();
    std::cout << "Configurations: ";
    for (const auto& config : configs) {
      std::cout << config << " ";
    }
    std::cout << std::endl;
    
    // Test link derivation names
    std::cout << "\nLink derivation names:" << std::endl;
    for (const auto& config : configs) {
      std::string name = GetDerivationNameForConfig("myapp", "", config);
      std::cout << "  " << config << ": " << name << std::endl;
    }
    
    // Test object derivation names
    std::cout << "\nObject derivation names for main.cpp:" << std::endl;
    for (const auto& config : configs) {
      std::string name = GetDerivationNameForConfig("myapp", "main.cpp", config);
      std::cout << "  " << config << ": " << name << std::endl;
    }
    
    // Test object derivation names for nested source
    std::cout << "\nObject derivation names for src/utils.cpp:" << std::endl;
    for (const auto& config : configs) {
      std::string name = GetDerivationNameForConfig("myapp", "src/utils.cpp", config);
      std::cout << "  " << config << ": " << name << std::endl;
    }
  }

private:
  cmake* CMakeInstance;
};

int main() {
  cmake cm;
  TestNixMultiGenerator gen(&cm);
  gen.TestNaming();
  return 0;
}