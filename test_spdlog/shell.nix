{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    # Build tools
    cmake
    gcc
    just
    
    # spdlog dependencies
    fmt  # spdlog uses fmt for formatting
  ];
}