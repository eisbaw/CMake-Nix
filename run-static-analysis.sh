#!/usr/bin/env bash
set -euo pipefail

# Run static analysis tools on the CMake Nix generator backend

echo "=== Running static analysis on CMake Nix Generator ==="
echo

# List of source files to analyze
NIX_GENERATOR_SOURCES=(
    "Source/cmGlobalNixGenerator.cxx"
    "Source/cmGlobalNixGenerator.h"
    "Source/cmLocalNixGenerator.cxx"
    "Source/cmLocalNixGenerator.h"
    "Source/cmNixTargetGenerator.cxx"
    "Source/cmNixTargetGenerator.h"
    "Source/cmNixCustomCommandGenerator.cxx"
    "Source/cmNixCustomCommandGenerator.h"
    "Source/cmNixPackageMapper.cxx"
    "Source/cmNixPackageMapper.h"
    "Source/cmGlobalNixMultiGenerator.cxx"
    "Source/cmGlobalNixMultiGenerator.h"
)

# Create a report directory
mkdir -p static-analysis-reports

# Run cppcheck
echo "=== Running cppcheck ==="
cppcheck --enable=all --suppress=missingIncludeSystem \
    --template=gcc \
    --report-progress \
    -I Source/ \
    -I Utilities/ \
    "${NIX_GENERATOR_SOURCES[@]}" \
    2>&1 | tee static-analysis-reports/cppcheck.log

echo
echo "=== Running clang-tidy ==="
# Create compile_commands.json if it doesn't exist
if [ ! -f compile_commands.json ]; then
    echo "Creating compile_commands.json..."
    # Use the built cmake
    if [ -f ./bin/cmake ]; then
        ./bin/cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON .
    else
        echo "Warning: cmake not found, skipping compile_commands.json generation"
    fi
fi

# Run clang-tidy on each source file
for source in "${NIX_GENERATOR_SOURCES[@]}"; do
    if [[ "$source" == *.cxx ]]; then
        echo "Analyzing $source..."
        clang-tidy "$source" \
            -checks='-*,bugprone-*,clang-analyzer-*,modernize-*,performance-*,readability-*,-modernize-use-trailing-return-type' \
            -header-filter='.*Nix.*\.h' \
            -- -std=c++17 -I Source/ -I Utilities/ \
            2>&1 | tee -a static-analysis-reports/clang-tidy.log || true
    fi
done

echo
echo "=== Summary ==="
echo "Reports saved to static-analysis-reports/"
echo
echo "Cppcheck warnings/errors:"
grep -E "(error|warning):" static-analysis-reports/cppcheck.log | wc -l || echo "0"
echo
echo "Clang-tidy warnings/errors:"
grep -E "(warning|error):" static-analysis-reports/clang-tidy.log 2>/dev/null | wc -l || echo "0"