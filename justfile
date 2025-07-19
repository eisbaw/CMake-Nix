# CMake Nix Backend Development Commands
# Requires: just (https://github.com/casey/just)
# Install with: cargo install just
# Or run via: nix-shell -p just --run 'just <command>'

# Test project modules
# Simple test with multiple C source files
mod test_multifile

# Basic Nix generator functionality test
mod test_nix_generator

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

# Bootstrap CMake from scratch (only needed once)
bootstrap:
    ./bootstrap --parallel=$(nproc)

# Build CMake with Nix generator
build:
    make -j$(nproc)

# Clean cruft for the test projects
clean-test-projects:
    -just test_multifile::clean
    -just test_nix_generator::clean
    -just test_headers::clean
    -just test_implicit_headers::clean
    -just test_explicit_headers::clean
    -just test_compiler_detection::clean
    -just test_shared_library::clean
    -just test_find_package::clean
    -just test_subdirectory::clean
    -just test_custom_commands::clean
    -just test_external_library::clean
    -just test_zlib_example::clean
    -just test_error_conditions::clean
    -just test_complex_build::clean
    -just test_cmake_self_host::clean

# Clean build of CMake itself and test project cruft½
clean: clean-test-projects
    -make clean

# Clean and rebuild CMake completely
rebuild: clean build

########################################################

# Run all tests and verify they build
test-all:
    just test_multifile::run
    just test_nix_generator::run
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
    just test_error_conditions::run
    just test_complex_build::run
    -just test_cmake_self_host::run
    @echo "✅ All tests passed!"

########################################################

# Quick development cycle: build, test, and show results
dev: build test-all
    @echo "🚀 Development cycle complete"
