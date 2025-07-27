#include <iostream>
#include <string>

std::string getConfig() {
#ifdef NDEBUG
  #ifdef CMAKE_BUILD_TYPE_MINSIZEREL
    return "MinSizeRel";
  #elif defined(CMAKE_BUILD_TYPE_RELWITHDEBINFO)
    return "RelWithDebInfo";
  #else
    return "Release";
  #endif
#else
    return "Debug";
#endif
}

int main() {
  std::cout << "Build configuration: " << getConfig() << std::endl;
  std::cout << "Optimization: ";
#ifdef __OPTIMIZE__
  std::cout << "Enabled";
#else
  std::cout << "Disabled";
#endif
  std::cout << std::endl;
  
  std::cout << "Debug info: ";
#ifdef _DEBUG
  std::cout << "Enabled";
#else
  std::cout << "Disabled";
#endif
  std::cout << std::endl;
  
  return 0;
}
