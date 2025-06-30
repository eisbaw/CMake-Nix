# Help - show available commands
help:
    @just --list

# CMake Nix Backend Development Commands
# Requires: just (https://github.com/casey/just)
# Install with: nix-env -iA nixpkgs.just
# Or: cargo install just
# Or run via: nix-shell -p just --run 'just <command>'

# Build CMake with Nix generator
build:
    cd CMake && make -j$(nproc)

# Clean and rebuild CMake completely
rebuild:
    -cd CMake && make clean
    cd CMake && make -j$(nproc)

# Bootstrap CMake from scratch (only needed once)
bootstrap:
    cd CMake && ./bootstrap --parallel=$(nproc)

########################################################

# Generate Nix files for simple multifile test
test-multifile:
    -rm -f test_multifile/default.nix
    cd test_multifile && ../CMake/bin/cmake -G Nix -DCMAKE_MAKE_PROGRAM=nix-build .

# Generate Nix files for headers test  
test-headers:
    -rm -f test_headers/default.nix
    cd test_headers && ../CMake/bin/cmake -G Nix -DCMAKE_MAKE_PROGRAM=nix-build .

# Generate Debug configuration for headers test
test-headers-debug:
    -rm -f test_headers/default.nix
    cd test_headers && ../CMake/bin/cmake -G Nix -DCMAKE_MAKE_PROGRAM=nix-build -DCMAKE_BUILD_TYPE=Debug .

# Generate Release configuration for headers test
test-headers-release:
    -rm -f test_headers/default.nix
    cd test_headers && ../CMake/bin/cmake -G Nix -DCMAKE_MAKE_PROGRAM=nix-build -DCMAKE_BUILD_TYPE=Release .

########################################################

# Build and run multifile calculator project
run-multifile: test-multifile
    cd test_multifile && nix-build -A calculator && ./result

# Build and run headers test project
run-headers: test-headers
    cd test_headers && nix-build -A test_headers && ./result

# Run all tests and verify they build
test-all: run-multifile run-headers
    @echo "✅ All tests passed!"

# Quick development cycle: build, test, and show results
dev: build test-all
    @echo "🚀 Development cycle complete"

# Clean all generated files
clean:
    -rm -f test_*/default.nix
    -rm -rf test_*/CMakeFiles test_*/CMakeCache.txt
    -cd CMake && make clean
