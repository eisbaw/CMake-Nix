with import <nixpkgs> {};
with lib;

let
  cmakeNixCC = {
    name,
    src,
    compiler ? gcc,
    flags ? "",
    source,
    buildInputs ? [],
    propagatedInputs ? []
  }: stdenv.mkDerivation {
    inherit name src buildInputs propagatedInputs;
    dontFixup = true;
    buildPhase = ''
      echo "Building ${name}"
      echo "Source: ${source}"
      touch $out
    '';
    installPhase = "true";
  };

  obj1 = cmakeNixCC {
    name = "test.o";
    src = ./.;
    buildInputs = [ gcc ];
    source = "main.c";
    compiler = gcc;
    flags = "-O3";
  };
in obj1