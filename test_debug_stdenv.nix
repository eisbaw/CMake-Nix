with import <nixpkgs> {};

stdenv.mkDerivation {
  name = "test-debug-stdenv";
  src = ./test_generator_expressions;
  buildPhase = ''
    mkdir -p $out
    
    # Test the condition
    echo "Testing compiler condition..."
    echo "Compiler: ${stdenv.cc}"
    echo "stdenv.cc: ${stdenv.cc}"
    
    if [ "${stdenv.cc}" = "${stdenv.cc}" ]; then
      echo "Condition TRUE: stdenv.cc equals stdenv.cc"
    else
      echo "Condition FALSE: stdenv.cc does not equal stdenv.cc"
    fi
    
    # Test if we have CC and CXX
    echo ""
    echo "CC = $CC"
    echo "CXX = $CXX"
    
    # Try compilation with CXX
    echo ""
    echo "Testing C++ compilation with CXX..."
    $CXX -c src/main.cpp -Iinclude -o $out/main.o
    echo "Success!"
  '';
}