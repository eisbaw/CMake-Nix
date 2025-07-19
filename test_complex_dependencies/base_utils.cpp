#include "base_utils.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>

namespace base {
    void Logger::log(const std::string& message) {
        std::cout << "[LOG] " << message << std::endl;
    }
    
    void Logger::debug(const std::string& message) {
        std::cout << "[DEBUG] " << message << std::endl;
    }
    
    std::string StringUtils::trim(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t\n\r");
        return str.substr(start, end - start + 1);
    }
    
    std::vector<std::string> StringUtils::split(const std::string& str, char delimiter) {
        std::vector<std::string> result;
        std::stringstream ss(str);
        std::string item;
        while (std::getline(ss, item, delimiter)) {
            result.push_back(item);
        }
        return result;
    }
    
    std::string StringUtils::join(const std::vector<std::string>& parts, const std::string& delimiter) {
        if (parts.empty()) return "";
        std::stringstream ss;
        ss << parts[0];
        for (size_t i = 1; i < parts.size(); ++i) {
            ss << delimiter << parts[i];
        }
        return ss.str();
    }
}