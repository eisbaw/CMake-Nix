with import <nixpkgs> {};

stdenv.mkDerivation {
  name = "test-cpp";
  src = ./test_generator_expressions;
  buildPhase = ''
    mkdir -p $out
    g++ -c src/main.cpp -Iinclude -o $out/main.o
  '';
}