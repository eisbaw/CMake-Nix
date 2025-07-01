cmake_minimum_required(VERSION 3.28)
project(test_zlib_example C)

# Test pure Nix library support with zlib
# This will trigger auto-generation of pkg_z.nix

add_executable(compress_demo main.c)

# Link against zlib directly
# Our Nix generator should:
# 1. Detect 'z' as an external library dependency
# 2. Look for pkg_z.nix  
# 3. Auto-generate it mapping to pkgs.zlib
# 4. Include it in buildInputs and link flags
target_link_libraries(compress_demo z)