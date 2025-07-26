with import <nixpkgs> {};

let
  compiler = gcc;
in
stdenv.mkDerivation {
  name = "test";
  dontUnpack = true;
  buildPhase = ''
    # This might fail
    echo "${compiler.pname or "cc"}"
    touch $out
  '';
}