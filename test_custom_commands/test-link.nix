with import <nixpkgs> {};
with lib;

let 
  obj1 = stdenv.mkDerivation {
    name = "obj1";
    dontUnpack = true;
    buildPhase = "echo obj1 > $out";
  };

  obj2 = stdenv.mkDerivation {
    name = "obj2";
    dontUnpack = true;
    buildPhase = "echo obj2 > $out";
  };

  cmakeNixLD = {
    name,
    type ? "executable",
    objects,
    compiler ? gcc,
    compilerCommand ? null,
    flags ? "",
    libraries ? [],
    buildInputs ? [],
    version ? null,
    soversion ? null,
    postBuildPhase ? ""
  }: stdenv.mkDerivation {
    inherit name objects buildInputs;
    dontUnpack = true;
    buildPhase = ''
      echo "Building ${name}"
      echo "Objects: $objects" 
      touch $out
    '';
    installPhase = "true";
  };

  link_app = cmakeNixLD {
    name = "app";
    type = "executable";
    buildInputs = [ gcc ];
    objects = [ obj1 obj2 ];
    compiler = gcc;
  };
in link_app