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

# Bootstrap CMake from scratch (only needed once)
bootstrap:
    ./bootstrap --parallel=$(nproc)

# Build CMake with Nix generator
build:
    make -j$(nproc)

# Clean cruft for the test projects
clean-test-projects:
    -just test_multifile::clean
    -just test_headers::clean
    -just test_implicit_headers::clean
    -just test_explicit_headers::clean

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
    @echo "✅ All tests passed!"

########################################################

# Quick development cycle: build, test, and show results
dev: build test-all
    @echo "🚀 Development cycle complete"
