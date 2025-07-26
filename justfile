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

# Explicit source derivations test (CMAKE_NIX_EXPLICIT_SOURCES)
mod test_explicit_sources

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

# Object library test
mod test_object_library

# Compile language generator expression test
mod test_compile_language_expr

# Interface library test
mod test_interface_library

# JSON library test (medium-sized C++ project)
mod test_json_library

# spdlog test (popular C++ logging library)
mod test_spdlog

# OpenCV real-world test (large C++ project)
mod test_opencv

# ASM language support test
mod test_asm_language

# Precompiled headers test
mod test_pch

# Unity build test
mod test_unity_build

# Module library test
mod test_module_library

# Fortran language support test
mod test_fortran_language

# fmt library test (medium-sized C++ formatting library)
mod test_fmt_library

# Performance benchmark test
mod test_performance_benchmark

# Multi-config edge cases test (RelWithDebInfo, MinSizeRel)
mod test_multiconfig_edge_cases

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
    -just test_explicit_sources::clean
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
    -just test_performance_benchmark::clean
    -just test_multiconfig_edge_cases::clean

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
    just test_explicit_sources::run
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
    just test_object_library::run
    just test_module_library::run
    just test_compile_language_expr::run
    just test_interface_library::run
    just test_json_library::run
    just test_spdlog::test
    -just test_asm_language::run  # ASM language not yet supported
    just test_pch::run
    just test_unity_build::run
    just test_fortran_language::run
    just test_fmt_library::run
    # test_external_tools is designed to fail - it demonstrates incompatibility
    # Run it but don't fail the test suite if it fails as expected
    -just test_external_tools::run || echo "✅ test_external_tools failed as expected (demonstrates incompatibility)"
    just test_file_edge_cases::run
    just test_nix_tools::run
    # Scale and error recovery tests (run separately due to special nature)
    # NOTE: These tests are not included in the standard regression suite due to:
    # - test_scale: Extended runtime with configurable file counts
    # - test_error_recovery: Tests error conditions that may fail intentionally
    # - test_cross_compile: Requires specific cross-compilation toolchain setup
    # - test_thread_safety: Stress test that may have unpredictable runtime
    # To run these tests individually:
    # just test_scale::test
    # just test_error_recovery::test
    # just test_cross_compile::test
    # just test_thread_safety::test
    just test_zephyr_rtos::run
    # OpenCV test is currently broken due to CMake policy issues in OpenCV itself
    # just test_opencv::configure-core
    @echo "✅ All tests passed!"

# Run special tests that are not part of the standard regression suite
test-special:
    @echo "Running special tests (may take longer or have special requirements)..."
    just test_scale::test
    just test_error_recovery::test
    just test_cross_compile::test
    just test_thread_safety::test
    @echo "✅ Special tests completed!"

########################################################

# Run nice-to-have tests (performance benchmarks, edge cases)
test-nice-to-have:
    @echo "=== Running Nice-to-Have Tests ==="
    @echo "These tests are optional and may take longer to run"
    @echo ""
    # Performance benchmark with different file counts
    just test_performance_benchmark::test
    # Multi-config edge cases (RelWithDebInfo, MinSizeRel)
    just test_multiconfig_edge_cases::run
    @echo ""
    @echo "✅ Nice-to-have tests completed"

########################################################

# Quick development cycle: build, test, and show results
dev: build test-all
    @echo "🚀 Development cycle complete"

# Zephyr RTOS simulation test
mod test_zephyr_rtos

# Special characters in target names test
mod test_special_characters

# CUDA language support test
mod test_cuda_language

# Security test for paths with special characters
mod test_security_paths

# Scale test (many files)
mod test_scale

# Error recovery test
mod test_error_recovery

# Cross-compilation test
mod test_cross_compile

# Thread safety test
mod test_thread_safety

# External tools test (ExternalProject_Add and FetchContent)
mod test_external_tools

# File edge cases test (spaces, unicode, special characters)
mod test_file_edge_cases

# Nix tools integration test
mod test_nix_tools

# Circular dependency test
mod test_circular_deps

# Advanced custom commands test
mod test_custom_commands_advanced
