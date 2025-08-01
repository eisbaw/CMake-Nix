# Generated by CMake Nix Generator
with import <nixpkgs> {};
with pkgs;

let
# Helper functions for DRY derivations

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
      compilerBin=$(
        if [[ "${compiler}" == "${gcc}" ]]; then
          echo "gcc"
        elif [[ "${compiler}" == "${clang}" ]]; then
          echo "clang"
        else
          echo "${compiler.pname or "cc"}"
        fi
      )
      ${compiler}/bin/$compilerBin -c ${flags} "${source}" -o "$out"
    '';
    installPhase = "true";
  };

  cmakeNixLD = {
    name,
    type ? "executable",  # "executable", "static", "shared", "module"
    objects,
    compiler ? gcc,
    flags ? "",
    libraries ? [],
    buildInputs ? [],
    version ? null,
    soversion ? null
  }: stdenv.mkDerivation {
    inherit name objects buildInputs;
    dontUnpack = true;
    buildPhase =
      if type == "static" then ''
        ar rcs "$out" $objects
      '' else if type == "shared" || type == "module" then ''
        mkdir -p $out
        compilerBin=$(
          if [[ "${compiler}" == "${gcc}" ]]; then
            echo "gcc"
          elif [[ "${compiler}" == "${clang}" ]]; then
            echo "clang"
          else
            echo "${compiler.pname or "cc"}"
          fi
        )
        libname="lib${name}.so"
        if [[ -n "${toString version}" ]]; then
          libname="lib${name}.so.${version}"
        fi
        ${compiler}/bin/$compilerBin -shared ${flags} $objects ${lib.concatMapStringsSep " " (l: l) libraries} -o "$out/$libname"
        # Create version symlinks if needed
        if [[ -n "${toString version}" ]]; then
          ln -sf "$libname" "$out/lib${name}.so"
          if [[ -n "${toString soversion}" ]]; then
            ln -sf "$libname" "$out/lib${name}.so.${soversion}"
          fi
        fi
      '' else ''
        compilerBin=$(
          if [[ "${compiler}" == "${gcc}" ]]; then
            echo "gcc"
          elif [[ "${compiler}" == "${clang}" ]]; then
            echo "clang"
          else
            echo "${compiler.pname or "cc"}"
          fi
        )
        ${compiler}/bin/$compilerBin ${flags} $objects ${lib.concatMapStringsSep " " (l: l) libraries} -o "$out"
      '';
    installPhase = "true";
  };

# Per-translation-unit derivations

  # Linking derivations
in
{
}
