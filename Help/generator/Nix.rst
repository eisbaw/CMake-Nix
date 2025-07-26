Nix
---

Generate Nix expressions for building C/C++ projects.

This generator produces a ``default.nix`` file that contains fine-grained 
Nix derivations for building C/C++ projects. Each source file gets its own
derivation, enabling maximum build parallelism and leveraging Nix's
advanced caching capabilities.

Platform Support
^^^^^^^^^^^^^^^^

**Important**: This generator is only supported on Unix/Linux systems where Nix is available.
Windows and macOS are not supported.

Generator Features
^^^^^^^^^^^^^^^^^^

The Nix generator supports:

- **Fine-grained parallelism**: One derivation per source file
- **Incremental builds**: Nix's content-addressed storage provides automatic caching
- **Reproducible builds**: Hermetic build environments ensure consistent results
- **Header dependency tracking**: Automatic detection and tracking of header files
- **External libraries**: Integration with system packages via ``find_package()``
- **Multiple configurations**: Debug, Release, RelWithDebInfo, MinSizeRel

Supported Languages
^^^^^^^^^^^^^^^^^^^

- C
- C++
- Fortran
- CUDA
- Assembly (ASM, ASM-ATT, ASM_NASM, ASM_MASM)
- Mixed language projects

Supported Target Types
^^^^^^^^^^^^^^^^^^^^^^

- Executables (``add_executable``)
- Static libraries (``add_library ... STATIC``)
- Shared libraries (``add_library ... SHARED``) with versioning support
- Custom targets with commands

Configuration and Building
^^^^^^^^^^^^^^^^^^^^^^^^^^

To configure a project with the Nix generator:

.. code-block:: console

  $ cmake -G Nix [options] <path-to-source>

The generator automatically detects and uses ``nix-build`` as the build program. 
If ``nix-build`` is not in your PATH, it will default to using ``nix-build`` anyway, 
assuming it will be available when needed.

To build targets, use ``nix-build`` with the appropriate target name:

.. code-block:: console

  $ nix-build -A target_name
  $ ./result

Build Configurations
^^^^^^^^^^^^^^^^^^^^

The generator supports multiple build configurations:

.. code-block:: console

  $ cmake -G Nix -DCMAKE_BUILD_TYPE=Debug .
  $ cmake -G Nix -DCMAKE_BUILD_TYPE=Release .
  $ cmake -G Nix -DCMAKE_BUILD_TYPE=RelWithDebInfo .
  $ cmake -G Nix -DCMAKE_BUILD_TYPE=MinSizeRel .

Generated Structure
^^^^^^^^^^^^^^^^^^^

The generator creates a ``default.nix`` file with the following structure:

.. code-block:: nix

  with import <nixpkgs> {};
  
  let
    # Per-translation-unit derivations
    main_c_o = stdenv.mkDerivation {
      name = "main.o";
      src = ./.;
      buildInputs = [ gcc ];
      buildPhase = ''
        gcc -c "main.c" -o "$out"
      '';
    };
    
    # Linking derivations  
    link_myapp = stdenv.mkDerivation {
      name = "myapp";
      buildInputs = [ gcc ];
      objects = [ main_c_o ];
      buildPhase = ''
        gcc $objects -o "$out"
      '';
    };
    
  in {
    "myapp" = link_myapp;
  }

Advanced Features
^^^^^^^^^^^^^^^^^

External Dependencies
~~~~~~~~~~~~~~~~~~~~~

The generator integrates with ``find_package()`` to automatically map
CMake packages to Nix packages:

.. code-block:: cmake

  find_package(ZLIB REQUIRED)
  target_link_libraries(myapp PRIVATE ZLIB::ZLIB)

This automatically includes the appropriate Nix package in the derivation.

Install Rules
~~~~~~~~~~~~~

The generator supports ``install()`` commands and creates separate install
derivations:

.. code-block:: cmake

  install(TARGETS myapp DESTINATION bin)
  install(TARGETS mylib DESTINATION lib)

Build the install derivation:

.. code-block:: console

  $ nix-build -A myapp_install

Custom Commands
~~~~~~~~~~~~~~~

Custom commands for code generation are supported:

.. code-block:: cmake

  add_custom_command(
    OUTPUT generated.c
    COMMAND generator input.txt generated.c
    DEPENDS input.txt generator
  )

The generator creates appropriate derivations for custom command outputs.

Subdirectories
~~~~~~~~~~~~~~

Multi-directory projects with ``add_subdirectory()`` are fully supported:

.. code-block:: cmake

  add_subdirectory(src)
  add_subdirectory(lib)

Performance Characteristics
^^^^^^^^^^^^^^^^^^^^^^^^^^^

The Nix generator provides significant performance benefits:

- **Parallel compilation**: All source files compile simultaneously
- **Incremental builds**: Only changed files are recompiled
- **Build caching**: Nix's content-addressed storage provides automatic caching
- **Distributed builds**: Can leverage Nix's remote build capabilities

Examples
^^^^^^^^

Basic Project
~~~~~~~~~~~~~

.. code-block:: cmake

  cmake_minimum_required(VERSION 3.20)
  project(Hello C)
  
  add_executable(hello main.c)

Configure and build:

.. code-block:: console

  $ cmake -G Nix .
  $ nix-build -A hello
  $ ./result

Library Project
~~~~~~~~~~~~~~~

.. code-block:: cmake

  cmake_minimum_required(VERSION 3.20)
  project(MyLib CXX)
  
  add_library(mylib SHARED src/library.cpp)
  add_executable(myapp src/main.cpp)
  target_link_libraries(myapp PRIVATE mylib)
  
  install(TARGETS myapp mylib
          RUNTIME DESTINATION bin
          LIBRARY DESTINATION lib)

Configure and build:

.. code-block:: console

  $ cmake -G Nix -DCMAKE_BUILD_TYPE=Release .
  $ nix-build -A myapp
  $ nix-build -A mylib  
  $ nix-build -A myapp_install