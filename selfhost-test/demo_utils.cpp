#include <fstream>

// Demo utility functions
bool fileExists(const std::string& filename) {
    std::ifstream file(filename);
    return file.good();
}

std::string getProjectInfo() {
    return "CMake Nix Generator Self-Hosting Demo v1.0";
}