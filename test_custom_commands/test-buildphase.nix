with import <nixpkgs> {};

stdenv.mkDerivation {
  name = "test";
  dontUnpack = true;
  buildPhase = ''
    compiler=${gcc}
    compilerBin=$(
      if [[ "$compiler" == "${gcc}" ]]; then
        echo "gcc"
      else
        echo "${gcc.pname or "cc"}"
      fi
    )
    echo "Compiler: $compiler"
    echo "CompilerBin: $compilerBin"
    touch $out
  '';
}