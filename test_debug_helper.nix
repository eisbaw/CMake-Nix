with import <nixpkgs> {};
with pkgs;
with lib;

let
  cmakeNixCC = {
    name,
    src,
    compiler ? gcc,
    flags ? "",
    source,  # Source file path relative to src
    buildInputs ? []
  }: stdenv.mkDerivation {
    inherit name src buildInputs;
    dontFixup = true;
    buildPhase = ''
      mkdir -p "$(dirname "$out")"
      # Store source in a variable to handle paths with spaces
      sourceFile="${source}"
      
      # Debug output
      echo "DEBUG: compiler = ${compiler}"
      echo "DEBUG: stdenv.cc = ${stdenv.cc}"
      echo "DEBUG: CC = $CC"
      echo "DEBUG: CXX = $CXX"
      
      # Determine how to invoke the compiler based on the compiler derivation
      # When using stdenv.cc, we use the environment variables $CC and $CXX
      # For other compilers, we use the binary directly
      if [ "${compiler}" = "${stdenv.cc}" ] || [ "${compiler}" = "${pkgsi686Linux.stdenv.cc}" ]; then
        # stdenv.cc provides $CC and $CXX environment variables
        echo "DEBUG: Using stdenv.cc path"
        if [[ "$sourceFile" == *.cpp ]] || [[ "$sourceFile" == *.cxx ]] || [[ "$sourceFile" == *.cc ]] || [[ "$sourceFile" == *.C ]]; then
          compilerCmd="$CXX"
        else
          compilerCmd="$CC"
        fi
      else
        echo "DEBUG: Using direct compiler path"
        # For other compilers, determine the binary name
        if [ "${compiler}" = "${gcc}" ] || [ "${compiler}" = "${pkgsi686Linux.gcc}" ]; then
          if [[ "$sourceFile" == *.cpp ]] || [[ "$sourceFile" == *.cxx ]] || [[ "$sourceFile" == *.cc ]] || [[ "$sourceFile" == *.C ]]; then
            compilerBin="g++"
          else
            compilerBin="gcc"
          fi
        elif [ "${compiler}" = "${clang}" ] || [ "${compiler}" = "${pkgsi686Linux.clang}" ]; then
          if [[ "$sourceFile" == *.cpp ]] || [[ "$sourceFile" == *.cxx ]] || [[ "$sourceFile" == *.cc ]] || [[ "$sourceFile" == *.C ]]; then
            compilerBin="clang++"
          else
            compilerBin="clang"
          fi
        elif [ "${compiler}" = "${gfortran}" ] || [ "${compiler}" = "${pkgsi686Linux.gfortran}" ]; then
          compilerBin="gfortran"
        else
          compilerBin="${compiler.pname or "cc"}"
        fi
        compilerCmd="${compiler}/bin/$compilerBin"
      fi
      # When src is a directory, Nix unpacks it into a subdirectory
      # We need to find the actual source file
      # Check if source is an absolute path or Nix expression (e.g., derivation/file)
      if [[ "$sourceFile" == /* ]] || [[ "$sourceFile" == *"\$"* ]]; then
        # Absolute path or Nix expression - use as-is
        srcFile="$sourceFile"
      elif [[ -f "$sourceFile" ]]; then
        srcFile="$sourceFile"
      elif [[ -f "$(basename "$src")/$sourceFile" ]]; then
        srcFile="$(basename "$src")/$sourceFile"
      else
        echo "Error: Cannot find source file $sourceFile"
        exit 1
      fi
      
      echo "DEBUG: compilerCmd = $compilerCmd"
      echo "DEBUG: Executing: $compilerCmd -c ${flags} \"$srcFile\" -o \"$out\""
      
      $compilerCmd -c ${flags} "$srcFile" -o "$out"
    '';
    installPhase = "true";
  };

in
  cmakeNixCC {
    name = "test-main.o";
    src = ./test_generator_expressions;
    compiler = stdenv.cc;
    source = "src/main.cpp";
    flags = "-Iinclude";
    buildInputs = [ stdenv.cc ];
  }