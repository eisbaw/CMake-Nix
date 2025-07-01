
cmake_minimum_required(VERSION 3.28)
project(test_external_library C)

# Test pure Nix library support without find_package()
# This will trigger our auto-generation of pkg_m.nix and pkg_pthread.nix

add_executable(main main.c)

# Link against external libraries directly
# Our Nix generator should:
# 1. Detect these as external library dependencies
# 2. Look for pkg_m.nix and pkg_pthread.nix  
# 3. Auto-generate them if missing
# 4. Include them in the linking derivation
target_link_libraries(main m pthread)
