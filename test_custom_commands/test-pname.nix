with import <nixpkgs> {};

let
  test1 = "${gcc.pname or "cc"}";
  test2 = gcc.pname or "cc";
in
stdenv.mkDerivation {
  name = "test";
  dontUnpack = true;
  buildPhase = ''
    echo "Test1: ${test1}"
    echo "Test2: ${test2}"
    echo "Direct: ${gcc.pname or "cc"}"
    touch $out
  '';
}