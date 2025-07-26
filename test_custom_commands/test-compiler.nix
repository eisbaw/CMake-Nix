with import <nixpkgs> {};

let
  cmakeNixCC = {
    name,
    src,
    compiler ? gcc,
    flags ? "",
    source,
    buildInputs ? [],
    propagatedInputs ? []
  }: 
  let
    compilerType = builtins.typeOf compiler;
  in
  stdenv.mkDerivation {
    inherit name src buildInputs propagatedInputs;
    dontFixup = true;
    buildPhase = ''
      echo "Compiler type: ${compilerType}"
      echo "Compiler: ${compiler}"
      touch $out
    '';
    installPhase = "true";
  };

  obj = cmakeNixCC {
    name = "test.o";
    src = ./.;
    buildInputs = [ gcc ];
    source = "main.c";
    compiler = gcc;
    flags = "-O3";
  };
in obj