#!/bin/bash
set -e

echo "Testing Nix Multi-Config Generator Implementation"
echo "================================================"

# Create a test project
TEST_DIR="test_multiconfig_manual"
rm -rf $TEST_DIR
mkdir -p $TEST_DIR
cd $TEST_DIR

# Create CMakeLists.txt
cat > CMakeLists.txt << 'EOF'
cmake_minimum_required(VERSION 3.28)
project(TestMultiConfig)

# Define configuration types
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)

# Add a simple executable
add_executable(myapp main.cpp)

# Set per-config compile definitions
target_compile_definitions(myapp PRIVATE
  $<$<CONFIG:Debug>:DEBUG_MODE>
  $<$<CONFIG:Release>:RELEASE_MODE>
  BUILD_CONFIG="${CMAKE_CFG_INTDIR}"
)

# Set per-config compile options
target_compile_options(myapp PRIVATE
  $<$<CONFIG:Debug>:-O0 -g>
  $<$<CONFIG:Release>:-O3>
)
EOF

# Create main.cpp
cat > main.cpp << 'EOF'
#include <iostream>

int main() {
    std::cout << "Multi-Config Test" << std::endl;
#ifdef DEBUG_MODE
    std::cout << "DEBUG configuration" << std::endl;
#endif
#ifdef RELEASE_MODE
    std::cout << "RELEASE configuration" << std::endl;
#endif
    return 0;
}
EOF

echo "Test project created in $TEST_DIR"
echo ""
echo "To test the generator, run:"
echo "  cd $TEST_DIR"
echo "  ../../Bootstrap.cmk/cmake -G \"Nix Multi-Config\" ."
echo ""
echo "Expected output in default.nix:"
echo "- Object derivations for both Debug and Release configs"
echo "- Link derivations for both Debug and Release configs"
echo "- Proper compile flags for each config (-O0 -g for Debug, -O3 for Release)"