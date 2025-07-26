#include <string>

std::string getConfigName() {
    // Return the configuration name from compile-time macro
    return CONFIG_NAME;
}