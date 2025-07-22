{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    # Build tools
    cmake
    gcc
    
    # Libraries that CMake should find
    zlib
    zlib.dev  # Include development headers
    libGL
    libGL.dev # Include OpenGL headers
    
    # For running tests
    just
  ];
  
  # Set up pkg-config and cmake paths
  shellHook = ''
    export CMAKE_PREFIX_PATH="${pkgs.zlib.dev}:${pkgs.libGL.dev}:$CMAKE_PREFIX_PATH"
    echo "Nix shell with ZLIB and OpenGL development packages"
  '';
}