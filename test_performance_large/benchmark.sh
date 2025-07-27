#!/usr/bin/env bash

set -e

echo "=== Performance Benchmark for Large Projects (1000+ files) ==="
echo ""
echo "Test configuration:"
echo "- 500 source files (.cpp)"
echo "- 500 header files (.h)"
echo "- 1 static library with all modules"
echo "- 1 executable linking the library"
echo ""

# Nix Generator Performance
echo "1. Nix Generator Performance:"
echo "------------------------------"
echo -n "Configuration time: "
START=$(date +%s.%N)
cd build && ../../bin/cmake -G Nix -DCMAKE_BUILD_TYPE=Release .. > /dev/null 2>&1
END=$(date +%s.%N)
echo "$(echo "$END - $START" | bc) seconds"

echo -n "Generation output size: "
ls -lh default.nix | awk '{print $5}'

echo -n "Build time (cold cache): "
rm -rf /tmp/nix-build-* 2>/dev/null || true
START=$(date +%s.%N)
nix-build -A perf_test --max-jobs auto > /dev/null 2>&1
END=$(date +%s.%N)
echo "$(echo "$END - $START" | bc) seconds"

echo -n "Build time (warm cache): "
START=$(date +%s.%N)
nix-build -A perf_test --max-jobs auto > /dev/null 2>&1
END=$(date +%s.%N)
echo "$(echo "$END - $START" | bc) seconds"

echo ""

# Unix Makefiles Performance
cd ..
echo "2. Unix Makefiles Generator Performance (for comparison):"
echo "---------------------------------------------------------"
rm -rf build_make
mkdir -p build_make
echo -n "Configuration time: "
START=$(date +%s.%N)
cd build_make && ../../bin/cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release .. > /dev/null 2>&1
END=$(date +%s.%N)
echo "$(echo "$END - $START" | bc) seconds"

echo -n "Makefile size: "
ls -lh Makefile | awk '{print $5}'

echo -n "Build time (parallel): "
START=$(date +%s.%N)
make -j$(nproc) > /dev/null 2>&1
END=$(date +%s.%N)
echo "$(echo "$END - $START" | bc) seconds"

echo ""

# Ninja Generator Performance (if available)
if command -v ninja >/dev/null 2>&1; then
    cd ..
    echo "3. Ninja Generator Performance (for comparison):"
    echo "-------------------------------------------------"
    rm -rf build_ninja
    mkdir -p build_ninja
    echo -n "Configuration time: "
    START=$(date +%s.%N)
    cd build_ninja && ../../bin/cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release .. > /dev/null 2>&1
    END=$(date +%s.%N)
    echo "$(echo "$END - $START" | bc) seconds"
    
    echo -n "build.ninja size: "
    ls -lh build.ninja | awk '{print $5}'
    
    echo -n "Build time (parallel): "
    START=$(date +%s.%N)
    ninja > /dev/null 2>&1
    END=$(date +%s.%N)
    echo "$(echo "$END - $START" | bc) seconds"
fi

echo ""
echo "4. File Statistics:"
echo "-------------------"
cd ..
echo "Total source files: $(find src -name '*.cpp' | wc -l)"
echo "Total header files: $(find src -name '*.h' | wc -l)"
echo "Total lines of code: $(find src -name '*.[ch]pp' -o -name '*.h' | xargs wc -l | tail -1)"

echo ""
echo "5. Key Observations:"
echo "--------------------"
echo "- The Nix generator creates fine-grained derivations for maximum parallelism"
echo "- Each source file can be compiled independently and cached"
echo "- Warm cache builds are nearly instantaneous due to Nix's content-addressed storage"
echo "- The generated Nix file is larger but provides better caching and reproducibility"