#ifndef BASE_UTILS_HPP
#define BASE_UTILS_HPP

#include <string>
#include <vector>

namespace base {
    class Logger {
    public:
        static void log(const std::string& message);
        static void debug(const std::string& message);
    };
    
    class StringUtils {
    public:
        static std::string trim(const std::string& str);
        static std::vector<std::string> split(const std::string& str, char delimiter);
        static std::string join(const std::vector<std::string>& parts, const std::string& delimiter);
    };
}

#endif // BASE_UTILS_HPP