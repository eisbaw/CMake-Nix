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
    buildInputs ? [],
    propagatedInputs ? []
  }: stdenv.mkDerivation {
    inherit name src buildInputs propagatedInputs;
    dontFixup = true;
    buildPhase = ''
      mkdir -p "$(dirname "$out")"
      compilerBin=$(
        if [[ "${compiler}" == "${gcc}" ]]; then
          echo "gcc"
        elif [[ "${compiler}" == "${clang}" ]]; then
          echo "clang"
        elif [[ "${compiler}" == "${gfortran}" ]]; then
          echo "gfortran"
        else
          echo "${compiler.pname or "cc"}"
        fi
      )
      # When src is a directory, Nix unpacks it into a subdirectory
      # We need to find the actual source file
      # Store source in a variable to handle paths with spaces
      sourceFile="${source}"
      # Check if source is an absolute path or Nix expression (e.g., ${derivation}/file)
      if [[ "$sourceFile" == /* ]] || [[ "$sourceFile" == *"$"* ]]; then
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
      ${compiler}/bin/$compilerBin -c ${flags} "$srcFile" -o "$out"
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
in app_test_custom_commands_main_c_o