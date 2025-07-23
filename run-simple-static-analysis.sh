#!/usr/bin/env bash
set -euo pipefail

echo "=== Running simple static analysis on CMake Nix Generator ==="
echo

# List of source files to analyze
NIX_GENERATOR_SOURCES=(
    "Source/cmGlobalNixGenerator.cxx"
    "Source/cmLocalNixGenerator.cxx"
    "Source/cmNixTargetGenerator.cxx"
    "Source/cmNixCustomCommandGenerator.cxx"
    "Source/cmNixPackageMapper.cxx"
    "Source/cmGlobalNixMultiGenerator.cxx"
)

echo "=== Checking for common issues ==="
echo

# Check for TODO/FIXME comments
echo "TODO/FIXME comments:"
grep -n "TODO\|FIXME" ${NIX_GENERATOR_SOURCES[@]} 2>/dev/null || echo "None found"
echo

# Check for debug output
echo "Debug output (cerr/cout):"
grep -n "std::cerr\|std::cout" ${NIX_GENERATOR_SOURCES[@]} 2>/dev/null | grep -v "NIX_DEBUG" || echo "None found (except debug-flagged)"
echo

# Check for memory leaks (basic check for new without delete)
echo "Potential memory issues (new without smart pointers):"
grep -n "new " ${NIX_GENERATOR_SOURCES[@]} 2>/dev/null | grep -v "unique_ptr\|shared_ptr\|make_unique" || echo "None found"
echo

# Check for hardcoded paths
echo "Hardcoded paths:"
grep -n '"/usr\|"/opt\|"/home' ${NIX_GENERATOR_SOURCES[@]} 2>/dev/null || echo "None found"
echo

# Check for exception safety
echo "Throw statements (for exception safety review):"
grep -n "throw " ${NIX_GENERATOR_SOURCES[@]} 2>/dev/null || echo "None found"
echo

echo "=== Summary complete ==="