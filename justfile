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

# Generate Nix files for implicit headers test (complex transitive dependencies)
test-implicit-headers:
    -rm -f test_implicit_headers/default.nix
    cd test_implicit_headers && ../CMake/bin/cmake -G Nix -DCMAKE_MAKE_PROGRAM=nix-build .

# Generate Debug configuration for implicit headers test
test-implicit-headers-debug:
    -rm -f test_implicit_headers/default.nix
    cd test_implicit_headers && ../CMake/bin/cmake -G Nix -DCMAKE_MAKE_PROGRAM=nix-build -DCMAKE_BUILD_TYPE=Debug .

# Generate Nix files for explicit headers test (manual OBJECT_DEPENDS)
test-explicit-headers:
    -rm -f test_explicit_headers/default.nix
    cd test_explicit_headers && ../CMake/bin/cmake -G Nix -DCMAKE_MAKE_PROGRAM=nix-build .

# Generate Debug configuration for explicit headers test
test-explicit-headers-debug:
    -rm -f test_explicit_headers/default.nix
    cd test_explicit_headers && ../CMake/bin/cmake -G Nix -DCMAKE_MAKE_PROGRAM=nix-build -DCMAKE_BUILD_TYPE=Debug .

########################################################

# Build and run multifile calculator project
run-multifile: test-multifile
    cd test_multifile && nix-build -A calculator && ./result

# Build and run headers test project
run-headers: test-headers
    cd test_headers && nix-build -A test_headers && ./result

# Build and run implicit headers test project
run-implicit-headers: test-implicit-headers
    cd test_implicit_headers && nix-build -A calc_app && ./result

# Build and run explicit headers test project  
run-explicit-headers: test-explicit-headers
    cd test_explicit_headers && nix-build -A ecs_app && ./result

# Run all tests and verify they build
test-all: run-multifile run-headers run-implicit-headers run-explicit-headers
    @echo "✅ All tests passed!"

########################################################

# View generated Nix file for multifile test
view-multifile: test-multifile
    cat test_multifile/default.nix

# View generated Nix file for headers test
view-headers: test-headers
    cat test_headers/default.nix

# View generated Nix file for implicit headers test
view-implicit-headers: test-implicit-headers
    cat test_implicit_headers/default.nix

# View generated Nix file for explicit headers test
view-explicit-headers: test-explicit-headers
    cat test_explicit_headers/default.nix

########################################################

# Quick development cycle: build, test, and show results
dev: build test-all
    @echo "🚀 Development cycle complete"

# Clean all generated files
clean:
    -rm -f test_*/default.nix
    -rm -rf test_*/CMakeFiles test_*/CMakeCache.txt
    -cd CMake && make clean
