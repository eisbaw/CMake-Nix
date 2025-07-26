cmake_minimum_required(VERSION 3.20)
project(MultiConfigEdgeCases CXX)

# Test edge cases for multi-configuration builds
# Specifically tests RelWithDebInfo and MinSizeRel configurations

set(CMAKE_CXX_STANDARD 17)

# Create a source file with configuration-specific behavior
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/config_test.cpp
  "#include <iostream>\n"
  "#include <string>\n"
  "\n"
  "std::string getConfig() {\n"
  "#ifdef NDEBUG\n"
  "  #ifdef CMAKE_BUILD_TYPE_MINSIZEREL\n"
  "    return \"MinSizeRel\";\n"
  "  #elif defined(CMAKE_BUILD_TYPE_RELWITHDEBINFO)\n"
  "    return \"RelWithDebInfo\";\n"
  "  #else\n"
  "    return \"Release\";\n"
  "  #endif\n"
  "#else\n"
  "    return \"Debug\";\n"
  "#endif\n"
  "}\n"
  "\n"
  "int main() {\n"
  "  std::cout << \"Build configuration: \" << getConfig() << std::endl;\n"
  "  std::cout << \"Optimization: \";\n"
  "#ifdef __OPTIMIZE__\n"
  "  std::cout << \"Enabled\";\n"
  "#else\n"
  "  std::cout << \"Disabled\";\n"
  "#endif\n"
  "  std::cout << std::endl;\n"
  "  \n"
  "  std::cout << \"Debug info: \";\n"
  "#ifdef _DEBUG\n"
  "  std::cout << \"Enabled\";\n"
  "#else\n"
  "  std::cout << \"Disabled\";\n"
  "#endif\n"
  "  std::cout << std::endl;\n"
  "  \n"
  "  return 0;\n"
  "}\n"
)

# Add executable with configuration-specific flags
add_executable(config_test ${CMAKE_CURRENT_BINARY_DIR}/config_test.cpp)

# Set configuration-specific compile definitions
target_compile_definitions(config_test PRIVATE
  $<$<CONFIG:MinSizeRel>:CMAKE_BUILD_TYPE_MINSIZEREL>
  $<$<CONFIG:RelWithDebInfo>:CMAKE_BUILD_TYPE_RELWITHDEBINFO>
)

# Add a library with different optimization settings per config
add_library(opt_lib STATIC
  ${CMAKE_CURRENT_BINARY_DIR}/config_test.cpp
)

# Test install rules with configuration-specific destinations
install(TARGETS config_test opt_lib
  RUNTIME DESTINATION bin/$<CONFIG>
  LIBRARY DESTINATION lib/$<CONFIG>
  ARCHIVE DESTINATION lib/$<CONFIG>
)