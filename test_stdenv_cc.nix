with import <nixpkgs> {};

stdenv.mkDerivation {
  name = "test-stdenv-cc";
  src = ./test_generator_expressions;
  buildPhase = ''
    mkdir -p $out
    # Use the wrapped compiler from stdenv
    $CXX -c src/main.cpp -Iinclude -o $out/main.o
    echo "Compilation successful!"
  '';
}