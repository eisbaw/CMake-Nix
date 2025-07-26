with import <nixpkgs> {};

stdenv.mkDerivation {
  name = "test";
  buildPhase = ''
    echo "Test: ${gcc}"
  '';
  dontUnpack = true;
}