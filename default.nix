{ pkgs ? import <nixpkgs> {} }:

let
  # Override the cmake package to use local source
  cmake-local = pkgs.cmake.overrideAttrs (oldAttrs: {
    # Use local source instead of fetchurl
    src = ./CMake;
    
    # Remove any patches that might not apply to our local version
    patches = [];
    
    # Ensure we have a clean build
    preConfigure = ''
      # Clean any existing build artifacts
      find . -name "CMakeCache.txt" -delete || true
      find . -name "CMakeFiles" -type d -exec rm -rf {} + || true
    '' + (oldAttrs.preConfigure or "");
    
    # Add version info for local build
    version = "local-${builtins.substring 0 8 (builtins.hashString "sha256" (builtins.readFile ./CMake/Source/cmVersion.cxx))}";
    
    # Ensure we can write to the source during build if needed
    postUnpack = ''
      chmod -R u+w $sourceRoot
    '' + (oldAttrs.postUnpack or "");
  });

in cmake-local
