#include "utils.hpp"
#include <iostream>
#include <sstream>

std::string get_feature_string() {
    std::stringstream ss;
    ss << "Utils function (C++17) - Version " << VERSION_MAJOR << "." << VERSION_MINOR;
#ifdef FEATURE_X_ENABLED
    ss << " [Feature X Enabled]";
#endif
    return ss.str();
}

int utility_compute(int x, int y) {
    std::cout << "Computing " << x << " + " << y << " = " << (x + y) << std::endl;
    return x + y;
}