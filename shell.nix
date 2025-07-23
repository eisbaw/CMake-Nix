{ pkgs ? (import (builtins.fetchTarball {
  url = "https://github.com/nixos/nixpkgs/tarball/25.05";
  sha256 = "1915r28xc4znrh2vf4rrjnxldw2imysz819gzhk9qlrkqanmfsxd";
}) {}) }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    just
    gcc
    gnumake
    zlib
    bzip2
    xz
    curl
    expat
    jsoncpp
    libarchive
    rhash
    libuv
    nghttp2
    zstd
    openssl
    pkg-config
  ];
} 
