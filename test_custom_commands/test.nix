with import <nixpkgs> {};

let
  obj1 = stdenv.mkDerivation {
    name = "obj1";
    buildPhase = "echo obj1 > $out";
    dontUnpack = true;
  };
  
  obj2 = stdenv.mkDerivation {
    name = "obj2";
    buildPhase = "echo obj2 > $out"; 
    dontUnpack = true;
  };
  
  app = stdenv.mkDerivation {
    name = "app";
    objects = [ obj1 obj2 ];
    buildInputs = [];
    dontUnpack = true;
    buildPhase = ''
      echo "Objects: $objects"
      cat $objects > $out
    '';
  };
in app