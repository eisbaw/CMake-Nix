# CMake Nix Generator Documentation

## Overview

The CMake Nix Generator is a production-ready CMake generator that produces Nix expressions for building C/C++ projects. It enables fine-grained derivations, maximum parallelism, and advanced caching through Nix's content-addressed storage.

## Table of Contents

1. [What is the Nix Generator](#what-is-the-nix-generator)
2. [Generated default.nix Structure](#generated-defaultnix-structure)
3. [Supported Features](#supported-features)
4. [How It Works](#how-it-works)
5. [Configuration Options](#configuration-options)
6. [Platform Support](#platform-support)
7. [Architecture](#architecture)
8. [Advanced Topics](#advanced-topics)

## What is the Nix Generator

The Nix Generator is a CMake generator backend that transforms CMake project definitions into Nix expressions. Unlike traditional generators that produce Makefiles or Ninja build files, the Nix generator creates a `default.nix` file containing fine-grained derivations for maximum build parallelism and caching efficiency.

### Key Benefits

- **Fine-grained parallelism**: Each source file compiles in its own derivation
- **Content-addressed caching**: Unchanged files never recompile
- **Reproducible builds**: Hermetic build environments ensure identical results
- **Distributed builds**: Leverage Nix's remote build capabilities
- **Pure dependency management**: External libraries handled through Nix packages

## Generated default.nix Structure

The Nix generator produces a single `default.nix` file with the following structure:

```nix
with import <nixpkgs> {};

let
  # Helper functions for compilation and linking
  cmakeNixCC = { name, source, flags, compiler, ... }: stdenv.mkDerivation { ... };
  cmakeNixLD = { name, type, objects, compiler, ... }: stdenv.mkDerivation { ... };

  # Per-source-file derivations (object files)
  myapp_main_cpp_o = cmakeNixCC {
    name = "main.o";
    source = "main.cpp";
    flags = "-O2 -DNDEBUG";
    compiler = gcc;
    headers = [ ./include/header.h ];
  };

  myapp_helper_cpp_o = cmakeNixCC {
    name = "helper.o";
    source = "src/helper.cpp";
    flags = "-O2 -DNDEBUG -Iinclude";
    compiler = gcc;
    headers = [ ./include/header.h ./include/types.h ];
  };

  # Library derivations
  link_mylib = cmakeNixLD {
    name = "mylib";
    type = "shared";
    objects = [ mylib_src_lib_cpp_o ];
    compiler = gcc;
    version = "1.0.0";
    soversion = "1";
  };

  # Executable derivations
  link_myapp = cmakeNixLD {
    name = "myapp";
    type = "executable";
    objects = [ myapp_main_cpp_o myapp_helper_cpp_o ];
    compiler = gcc;
    libraries = [ "${link_mylib}/libmylib.so" ];
  };

in {
  # Top-level targets exposed for nix-build
  myapp = link_myapp;
  mylib = link_mylib;
}
```

### Key Components

1. **Helper Functions**
   - `cmakeNixCC`: Handles compilation of individual source files
   - `cmakeNixLD`: Handles linking of executables and libraries

2. **Object Derivations**
   - One derivation per source file
   - Includes precise header dependencies
   - Uses fileset unions to minimize Nix store usage

3. **Link Derivations**
   - Separate derivations for linking
   - Support for executables, static libraries, shared libraries, and modules
   - Proper handling of library versioning

4. **Output Attribute Set**
   - Maps CMake target names to Nix derivations
   - Allows selective building with `nix-build -A targetname`

## Supported Features

### Language Support
- ✅ C (all standards)
- ✅ C++ (all standards)
- ✅ Fortran
- ✅ CUDA
- ✅ Assembly (ASM, ASM-ATT, ASM_NASM, ASM_MASM)
- ❌ Swift (not supported - requires CMake changes)

### Target Types
- ✅ Executables (`add_executable`)
- ✅ Static libraries (`add_library ... STATIC`)
- ✅ Shared libraries (`add_library ... SHARED`) with versioning
- ✅ Module libraries (`add_library ... MODULE`)
- ✅ Object libraries (`add_library ... OBJECT`)
- ✅ Interface libraries (`add_library ... INTERFACE`)

### Build Features
- ✅ Multi-directory projects (`add_subdirectory`)
- ✅ Custom commands (`add_custom_command`)
- ✅ Custom targets (`add_custom_target`)
- ✅ Install rules (`install()`)
- ✅ Multiple build configurations (Debug, Release, etc.)
- ✅ Generator expressions
- ✅ Precompiled headers (PCH)
- ✅ Header dependency tracking
- ✅ External package integration (`find_package`)

### Special Features
- ✅ Fine-grained source filtering with Nix filesets
- ✅ Automatic compiler detection
- ✅ Cross-compilation support (basic)
- ✅ Compile definitions and options
- ✅ Target properties
- ✅ Transitive dependencies

### Limitations
- ❌ Unity builds (warning issued, not beneficial with Nix)
- ❌ Windows/macOS specific features (Nix is Unix/Linux only)
- ❌ Compile commands export (not applicable to Nix)
- ❌ Response files (not needed - commands in derivation scripts)

## How It Works

### 1. Configuration Phase

During CMake configuration, the Nix generator:
- Analyzes all targets and their dependencies
- Detects compilers and build tools
- Scans for header dependencies
- Builds a dependency graph

### 2. Generation Phase

The generator creates a `default.nix` file by:
- Creating one derivation per translation unit
- Generating link derivations for each target
- Setting up proper dependency chains
- Creating helper functions for common patterns

### 3. Build Phase

When you run `nix-build`:
- Nix evaluates the expressions
- Builds derivations in parallel where possible
- Caches results based on content hashes
- Links the final outputs

### Main Code Flow

```
cmGlobalNixGenerator::Generate()
├── CollectTargets()
├── BuildDependencyGraph()
├── GenerateCustomCommands()
├── WriteNixFile()
│   ├── WriteHelperFunctions()
│   ├── WriteObjectDerivations()
│   │   └── For each source file:
│   │       ├── DetermineCompilerPackage()
│   │       ├── GetCompileFlags()
│   │       ├── ProcessHeaderDependencies()
│   │       └── WriteFilesetUnion()
│   ├── WriteLinkDerivations()
│   │   └── For each target:
│   │       ├── CollectObjects()
│   │       ├── ProcessLibraryDependencies()
│   │       └── GenerateLinkCommand()
│   └── WriteOutputSet()
└── WriteCustomCommands()
```

### Key Data Structures

1. **cmNixDependencyGraph**
   - Tracks target-to-target dependencies
   - Provides transitive dependency queries
   - Detects circular dependencies

2. **TransitiveDependencyCache**
   - Caches header dependency scanning results
   - Prevents redundant filesystem operations
   - Thread-safe with mutex protection

3. **CustomCommands**
   - Stores custom command definitions
   - Maintains topological order for execution
   - Maps outputs to generating commands

## Configuration Options

### CMake Variables

```cmake
# Enable explicit source tracking (more precise rebuilds)
set(CMAKE_NIX_EXPLICIT_SOURCES ON)

# Override compiler packages
set(CMAKE_NIX_C_COMPILER_PACKAGE "clang")
set(CMAKE_NIX_CXX_COMPILER_PACKAGE "clang")

# Override compiler commands
set(CMAKE_NIX_C_COMPILER_COMMAND "clang-16")
set(CMAKE_NIX_CXX_COMPILER_COMMAND "clang++-16")

# Define additional system path prefixes
set(CMAKE_NIX_SYSTEM_PATH_PREFIXES "/opt/local;/usr/local")

# Enable debug output
set(CMAKE_NIX_DEBUG ON)
```

### Generator-Specific Features

```bash
# Single-configuration build
cmake -G Nix -DCMAKE_BUILD_TYPE=Release .

# Multi-configuration build
cmake -G "Nix Multi-Config" .
```

## Platform Support

The Nix generator is designed specifically for Unix/Linux platforms where Nix runs:

- **Primary targets**: Linux distributions, NixOS
- **Secondary targets**: macOS (with Nix installed)
- **Not supported**: Windows, other platforms

### Platform-Specific Notes

1. **Library naming**: Uses Unix conventions (lib*.so, lib*.a)
2. **Path separators**: Unix-style forward slashes
3. **System headers**: Filters common Unix system paths
4. **Compiler detection**: Optimized for GCC/Clang

## Architecture

### Class Hierarchy

```
cmGlobalGenerator
└── cmGlobalCommonGenerator
    └── cmGlobalNixGenerator
        └── cmGlobalNixMultiGenerator

cmLocalGenerator
└── cmLocalNixGenerator

cmNixTargetGenerator
cmNixCustomCommandGenerator
cmNixPackageMapper
cmNixCompilerResolver
cmNixWriter
cmNixDependencyGraph
```

### Key Components

1. **cmGlobalNixGenerator**
   - Main generator implementation
   - Orchestrates the generation process
   - Manages global state and caches

2. **cmLocalNixGenerator**
   - Per-directory generation logic
   - Creates target generators

3. **cmNixTargetGenerator**
   - Per-target Nix expression generation
   - Header dependency scanning
   - Compile flag computation

4. **cmNixCustomCommandGenerator**
   - Handles add_custom_command directives
   - Generates Nix derivations for custom build steps

5. **cmNixPackageMapper**
   - Maps CMake package names to Nix packages
   - Handles find_package integration

6. **cmNixCompilerResolver**
   - Detects and configures compilers
   - Maps compiler IDs to Nix packages
   - Supports user overrides

## Advanced Topics

### Fine-Grained Caching

The generator uses Nix fileset unions to create minimal source sets:

```nix
src = fileset.toSource {
  root = ./.;
  fileset = fileset.unions [
    ./main.cpp
    ./include/header.h
    ./include/types.h
  ];
};
```

This ensures rebuilds only when relevant files change.

### External Dependencies

External libraries are handled through package files:

```nix
# pkg_ZLIB.nix
{ zlib }:
{
  buildInputs = [ zlib ];
  linkFlags = "-lz";
}
```

### Custom Commands

Custom commands become separate derivations:

```nix
custom_generate_version_h = stdenv.mkDerivation {
  name = "version.h";
  buildCommand = ''
    echo "#define VERSION \"1.0\"" > $out
  '';
};
```

### Multi-Configuration Support

The Nix Multi-Config generator creates separate derivations per configuration:

```nix
myapp_Debug = cmakeNixLD { flags = "-g -O0"; ... };
myapp_Release = cmakeNixLD { flags = "-O3 -DNDEBUG"; ... };
```

### Error Handling

The generator includes comprehensive error checking:
- Path validation to prevent directory traversal
- Circular dependency detection
- Missing source file warnings
- Invalid target name sanitization

### Performance Optimizations

1. **Caching**: Transitive dependencies cached to avoid redundant scans
2. **Parallel Generation**: Thread-safe caches allow parallel processing
3. **Lazy Evaluation**: Nix evaluates only required derivations
4. **Content Addressing**: Identical content reuses existing store paths

## Best Practices

1. **Use CMAKE_NIX_EXPLICIT_SOURCES** for precise dependency tracking
2. **Organize headers** in dedicated include directories
3. **Avoid absolute paths** in CMakeLists.txt files
4. **Use find_package** for external dependencies
5. **Leverage Nix flakes** for reproducible development environments

## Troubleshooting

### Common Issues

1. **"file does not exist" errors**
   - Ensure generated files are created during configuration
   - Check custom command dependencies

2. **Missing dependencies**
   - Verify find_package modules are available
   - Check pkg_*.nix files are generated

3. **Slow builds**
   - Enable CMAKE_NIX_EXPLICIT_SOURCES
   - Check for unnecessary header includes

### Debug Mode

Enable debug output to diagnose issues:

```bash
export CMAKE_NIX_DEBUG=1
cmake -G Nix .
```

This will print detailed information about:
- Dependency graph construction
- Header scanning results
- Compiler detection
- Package mapping decisions