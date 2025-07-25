{ pkgs ? (import (builtins.fetchTarball {
  url = "https://github.com/nixos/nixpkgs/tarball/25.05";
  sha256 = "1915r28xc4znrh2vf4rrjnxldw2imysz819gzhk9qlrkqanmfsxd";
}) {}) }:

pkgs.mkShell {
  nativeBuildInputs = with pkgs; [
    just
    gcc
    gfortran
    gnumake
    pkg-config
    # Static analysis and linting tools
    clang-tools  # clang-tidy
    cppcheck
    include-what-you-use
    valgrind
  ];
  
  buildInputs = with pkgs; [
    zlib.dev
    zlib
    bzip2
    xz
    curl.dev
    curl
    expat
    jsoncpp
    libarchive.dev
    libarchive
    libuv.dev
    libuv
    nghttp2
    zstd
    openssl.dev
    openssl
  ];
  
  # Help CMake find system libraries during bootstrap
  shellHook = ''
    export CMAKE_PREFIX_PATH="${pkgs.zlib.dev}:${pkgs.curl.dev}:${pkgs.libarchive.dev}:${pkgs.libuv.dev}:${pkgs.openssl.dev}"
    export PKG_CONFIG_PATH="${pkgs.zlib.dev}/lib/pkgconfig:${pkgs.curl.dev}/lib/pkgconfig:${pkgs.libarchive.dev}/lib/pkgconfig:${pkgs.libuv.dev}/lib/pkgconfig:${pkgs.openssl.dev}/lib/pkgconfig"
    export OPENSSL_ROOT_DIR="${pkgs.openssl.dev}"
  '';
} 
