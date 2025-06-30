Nix
---

Generates Nix expressions for building C/C++ projects with fine-grained 
derivations that maximize build parallelism and caching efficiency.

The Nix generator produces a ``default.nix`` file containing:

* One derivation per translation unit (source file)
* Separate linking derivations for executables and libraries  
* Top-level attribute set exposing final targets

Generated Nix expressions can be built using ``nix-build``.

Example Usage
^^^^^^^^^^^^^

Configure a project to use the Nix generator::

  cmake -G Nix -DCMAKE_MAKE_PROGRAM=nix-build /path/to/source

Build targets with nix-build::

  nix-build -A target_name

Generated Structure
^^^^^^^^^^^^^^^^^^^

For a simple project with ``main.c``, the generator creates::

  # default.nix
  with import <nixpkgs> {};
  
  let
    # Per-translation-unit derivations
    hello_main_c_o = stdenv.mkDerivation {
      name = "main.o";
      src = ./main.c;
      buildInputs = [ gcc ];
      buildPhase = ''gcc -c "$src" -o "$out"'';
    };
    
    # Linking derivations
    link_hello = stdenv.mkDerivation {
      name = "hello";
      objects = [ hello_main_c_o ];
      buildPhase = ''gcc $objects -o "$out"'';
    };
  in
  {
    "hello" = link_hello;
  }

Features
^^^^^^^^

* **Fine-grained parallelism**: Each source file compiles independently
* **Nix caching**: Object files are cached between builds  
* **Multiple targets**: Supports executables and static libraries
* **Reproducible builds**: Leverages Nix's hermetic build environment

Current Limitations
^^^^^^^^^^^^^^^^^^^

* Header dependencies are not tracked
* Include paths are not handled
* Only single-configuration builds supported
* External libraries not yet integrated

This generator is in active development. See the CMake Nix Backend PRD 
for planned enhancements.