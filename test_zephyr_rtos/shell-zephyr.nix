{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    # Basic build tools
    gcc
    gnumake
    cmake
    ninja
    python3
    git
    just
    
    # Zephyr-specific dependencies
    python3Packages.west
    python3Packages.pyelftools
    python3Packages.pyyaml
    python3Packages.pykwalify
    python3Packages.cbor2
    python3Packages.packaging
    python3Packages.progress
    python3Packages.psutil
    python3Packages.pylink-square
    python3Packages.anytree
    python3Packages.intelhex
    
    # Device tree compiler
    dtc
    
    # Documentation tools (optional but often needed)
    doxygen
    
    # Additional tools
    gperf
    ccache
    
    # For native/posix builds
    SDL2  # For display/input simulation
    
    # Nix-specific tools for our backend
    nix
    nix-prefetch-git
    
    # Debugging tools
    gdb
  ];
  
  shellHook = ''
    echo "Zephyr RTOS development environment loaded"
    echo "This shell is configured for native/POSIX builds"
    echo ""
    echo "To initialize a Zephyr workspace:"
    echo "  west init -l ."
    echo "  west update"
    echo ""
    echo "To build with CMake Nix backend:"
    echo "  cmake -G Nix -DBOARD=native_sim -DCMAKE_MAKE_PROGRAM=nix-build ."
    echo "  nix-build -A app"
    
    # Set up Zephyr environment variables
    export ZEPHYR_BASE=$(pwd)/zephyr
    export ZEPHYR_TOOLCHAIN_VARIANT=host
  '';
}