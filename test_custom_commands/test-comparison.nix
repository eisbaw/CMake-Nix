with import <nixpkgs> {};

let
  compiler = gcc;
in
stdenv.mkDerivation {
  name = "test";
  buildPhase = ''
    if [[ "${compiler}" == "${gcc}" ]]; then
      echo "GCC matched"
    fi
    echo "Done" > $out
  '';
  dontUnpack = true;
}