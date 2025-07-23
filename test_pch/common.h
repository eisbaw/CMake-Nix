#ifndef COMMON_H
#define COMMON_H

// Common includes used across the project
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>

// Common constants
const int DEFAULT_SIZE = 100;
const std::string APP_NAME = "PCH Test Application";

// Common utility functions
inline void print_header() {
    std::cout << "=== " << APP_NAME << " ===" << std::endl;
}

#endif // COMMON_H