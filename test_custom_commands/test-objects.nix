with import <nixpkgs> {};
with pkgs;
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
      echo "Test build ${name}"
      touch $out
    '';
    installPhase = "true";
  };

  app_test_custom_commands_main_c_o = cmakeNixCC {
    name = "main.o";
    src = ./.;
    buildInputs = [ gcc ];
    source = "main.c";
    compiler = gcc;
    flags = "-O3 -DNDEBUG";
  };
  
  # Test if we can evaluate it
  test = builtins.typeOf app_test_custom_commands_main_c_o;
in test