with import <nixpkgs> {};

stdenv.mkDerivation {
  name = "test-debug-generated";
  src = ./test_generator_expressions;
  buildPhase = ''
    mkdir -p $out
    
    # Check what compiler is being used
    compiler="${stdenv.cc}"
    echo "compiler = $compiler"
    echo "stdenv.cc = ${stdenv.cc}"
    
    # Check the path
    if [ -e "$compiler/bin/g++" ]; then
      echo "Found g++ at: $compiler/bin/g++"
      echo "Running: $compiler/bin/g++ --version"
      $compiler/bin/g++ --version
    else
      echo "ERROR: g++ not found at $compiler/bin/g++"
    fi
    
    # Try compiling with the wrapped compiler
    echo ""
    echo "Trying compilation..."
    $compiler/bin/g++ -c src/main.cpp -Iinclude -o $out/main.o
    echo "Success!"
  '';
}