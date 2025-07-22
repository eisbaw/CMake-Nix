# CMake Nix Backend Development Commands
# Requires: just (https://github.com/casey/just)
# Install with: cargo install just
# Or run via: nix-shell -p just --run 'just <command>'

# Test project modules
# Simple test with multiple C source files
mod test_multifile

# Test project with separate header files  
mod test_headers

# Complex transitive dependencies test
mod test_implicit_headers

# Manual OBJECT_DEPENDS configuration test
mod test_explicit_headers

# Compiler auto-detection test
mod test_compiler_detection

# Shared library support test
mod test_shared_library

# find_package() integration test
mod test_find_package

# Subdirectory support test
mod test_subdirectory

# Custom command support test  
mod test_custom_commands

# External library integration test
mod test_external_library

# Zlib external library example
mod test_zlib_example

# Error condition and edge case tests
mod test_error_conditions

# Complex build configuration tests
mod test_complex_build

# CMake self-hosting test
mod test_cmake_self_host

# Multi-configuration build test
mod test_multiconfig

# Install rules test
mod test_install_rules

# Mixed language support test
mod test_mixed_language

# Preprocessor definitions test
mod test_preprocessor

# Complex dependencies test
mod test_complex_dependencies

# Package integration test
mod test_package_integration

# Compiler feature detection test
mod test_feature_detection

# OpenCV real-world test (large C++ project)
mod test_opencv

# Bootstrap CMake from scratch (only needed once)
bootstrap:
    ./bootstrap --parallel=$(nproc) --no-system-curl -- -DCMAKE_USE_OPENSSL=OFF -DCMAKE_USE_SYSTEM_ZLIB=OFF

# Build CMake with Nix generator
build:
    make -j$(nproc)

# Clean cruft for the test projects
clean-test-projects:
    -just test_multifile::clean
    -just test_headers::clean
    -just test_implicit_headers::clean
    -just test_explicit_headers::clean
    -just test_compiler_detection::clean
    -just test_shared_library::clean
    -just test_find_package::clean
    -just test_custom_commands::clean
    -just test_external_library::clean
    -just test_zlib_example::clean
    -just test_error_conditions::clean
    -just test_complex_build::clean
    -just test_cmake_self_host::clean
    -just test_multiconfig::clean
    -just test_install_rules::clean
    -just test_mixed_language::clean
    -just test_preprocessor::clean
    -just test_complex_dependencies::clean
    -just test_package_integration::clean
    -just test_feature_detection::clean
    -just test_opencv::clean

# Clean build of CMake itself and test project cruft½
clean: clean-test-projects
    -make clean

# Clean and rebuild CMake completely
rebuild: clean build

########################################################

# Run all tests and verify they build
test-all:
    just test_multifile::run
    just test_headers::run
    just test_implicit_headers::run
    just test_explicit_headers::run
    just test_compiler_detection::run
    just test_shared_library::run
    just test_find_package::run
    just test_subdirectory::run
    just test_custom_commands::run
    just test_external_library::run
    just test_zlib_example::run
    -just test_error_conditions::run
    just test_complex_build::run
    just test_multiconfig::run
    just test_install_rules::run
    just test_mixed_language::run
    just test_preprocessor::run
    just test_complex_dependencies::run
    just test_package_integration::run
    just test_feature_detection::run
    # OpenCV test is currently broken due to CMake policy issues in OpenCV itself
    # just test_opencv::configure-core
    @echo "✅ All tests passed!"

########################################################

# Quick development cycle: build, test, and show results
dev: build test-all
    @echo "🚀 Development cycle complete"
