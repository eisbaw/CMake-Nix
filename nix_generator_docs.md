# CMake Nix Generator Documentation

## Overview

The CMake Nix Generator is a custom CMake generator that produces Nix expressions instead of traditional build files like Makefiles. It enables fine-grained build caching and parallelism by creating one Nix derivation per translation unit.

## Generated default.nix Structure

The generator produces a single `default.nix` file with the following structure:

```nix
{ pkgs ? import <nixpkgs> {} }:
with pkgs;
let
  # Helper functions
  cmakeNixCC = { ... };  # Compiler wrapper for object files
  cmakeNixLD = { ... };  # Linker wrapper for executables/libraries
  
  # Object derivations (one per source file)
  main_c_o = cmakeNixCC {
    name = "main.c.o";
    src = ./.;
    compiler = gcc;
    flags = "-O2 -DNDEBUG -Iinclude";
    source = "src/main.c";
  };
  
  # Link derivations
  myapp = cmakeNixLD {
    name = "myapp";
    type = "executable";
    objects = [ main_c_o utils_c_o ];
    compiler = gcc;
    flags = "-O2";
    libraries = [ "-lm" "-lpthread" ];
  };
  
in {
  # Exported targets
  inherit myapp;
  all = stdenv.mkDerivation {
    name = "all";
    buildInputs = [ myapp ];
    ...
  };
}
```

## Key Features

### 1. Fine-Grained Derivations
- One derivation per `.c`, `.cpp`, `.cxx`, `.cc` source file
- Enables maximum parallelism and caching
- Only rebuilds changed translation units

### 2. Helper Functions
- `cmakeNixCC`: Standardized object compilation
  - Handles C, C++, Fortran, Assembly
  - Auto-detects compiler binary name
  - Supports custom flags per source
- `cmakeNixLD`: Standardized linking
  - Supports executables, static/shared libraries, modules
  - Handles library versioning and symlinks
  - Manages library dependencies

### 3. Source Handling
- Relative paths preserved in derivations
- External sources supported via composite source sets
- Custom command outputs integrated
- Configuration-time generated files embedded

### 4. Multi-Configuration Support
- Debug/Release configurations detected
- Configuration-specific flags applied
- Preprocessor definitions included

## Architecture

### Core Classes

1. **cmGlobalNixGenerator** (inherits from cmGlobalCommonGenerator)
   - Main generator class
   - Coordinates Nix file generation
   - Manages global state and helper functions

2. **cmLocalNixGenerator** (inherits from cmLocalGenerator)
   - Per-directory generation
   - Handles local target processing

3. **cmNixTargetGenerator**
   - Per-target Nix derivation generation
   - Processes source files and dependencies
   - Generates object and link derivations

4. **cmNixPackageMapper**
   - Maps CMake targets to Nix packages
   - Handles system library mappings
   - Provides link flags for targets

5. **cmNixWriter**
   - Utility class for writing Nix expressions
   - Handles proper indentation and escaping
   - Provides convenience methods for common patterns

### Main Code Flow

1. **Generator Registration**
   ```cpp
   cmake::AddDefaultGenerators()
   → cmGlobalNixGenerator::NewFactory()
   → Generator available as "Nix"
   ```

2. **Configuration Phase**
   ```cpp
   cmGlobalNixGenerator::Configure()
   → Standard CMake configuration
   → Target discovery and analysis
   ```

3. **Generation Phase**
   ```cpp
   cmGlobalNixGenerator::Generate()
   → WriteNixPreamble()       // Nix imports and setup
   → WriteHelperFunctions()   // cmakeNixCC, cmakeNixLD
   → WriteDependencies()      // Process all targets
     → cmNixTargetGenerator::Generate()
       → WriteObjectDerivations()  // Per-source derivations
       → WriteLinkDerivation()     // Final executable/library
   → WriteTargetExports()     // Export public targets
   ```

### Key Data Structures

1. **ObjectDerivationCache**
   - Maps source files to derivation names
   - Prevents duplicate derivations
   - Thread-safe with mutex protection

2. **TransitiveDependencyCache**
   - Caches computed transitive dependencies
   - Improves performance for complex graphs
   - Size-limited to prevent unbounded growth

3. **Package Mappings**
   - Target name → Nix package name
   - Target name → Link flags
   - Extensible for custom mappings

## Supported CMake Features

### ✅ Implemented
- Basic executable targets
- Static libraries
- Shared libraries (with versioning)
- Object libraries
- Module libraries
- Include directories (`target_include_directories`)
- Compile definitions (`target_compile_definitions`)
- Compile options (`target_compile_options`)
- Link libraries (internal targets)
- Configuration types (Debug/Release)
- Custom commands (basic support)
- Assembly language support
- Fortran support

### ⚠️ Partial Support
- External libraries (limited mappings)
- Header dependency tracking (basic)
- Compiler detection (GNU/Clang only)
- Multi-configuration generators

### ❌ Not Implemented
- find_package() integration
- pkg-config support
- Cross-compilation
- Install rules
- CPack integration
- Unity builds
- Precompiled headers
- Windows/macOS specific features

## Usage

```bash
# Configure with Nix generator
cmake -G Nix -DCMAKE_BUILD_TYPE=Release .

# Build all targets
nix-build

# Build specific target
nix-build -A myapp

# Clean build
rm -rf result default.nix
```

## Configuration Options

- `CMAKE_NIX_EXPLICIT_SOURCES`: Enable explicit source listing
- `CMAKE_NIX_IGNORE_CIRCULAR_DEPS`: Bypass circular dependency checks
- `CMAKE_BUILD_TYPE`: Set build configuration (Debug/Release)

## Limitations and Assumptions

1. **Platform Support**: Unix-like systems only (Linux, macOS)
2. **Compiler Support**: GCC and Clang primarily tested
3. **Path Handling**: Assumes Unix-style paths
4. **Library Naming**: Unix conventions (lib*.so, lib*.a)
5. **System Headers**: Limited detection (/usr/include, /nix/store)
6. **Package Mappings**: Hardcoded for common libraries

## Extending the Generator

### Adding Package Mappings
Edit `cmNixPackageMapper::InitializeMappings()`:
```cpp
this->TargetToNixPackage["MyLib::MyLib"] = "mylib";
this->TargetToLinkFlags["MyLib::MyLib"] = "-lmylib";
```

### Supporting New Languages
1. Add language detection in `DetermineCompilerPackage()`
2. Update compiler command in `GetCompilerCommand()`
3. Add buildInputs mapping for language tools

### Custom Build Rules
Override methods in cmNixTargetGenerator:
- `WriteObjectDerivation()` for compilation
- `WriteLinkDerivation()` for linking

## Debugging

Enable CMake trace output:
```bash
cmake -G Nix --trace-expand .
```

Inspect generated Nix:
```bash
nix-instantiate --eval --strict default.nix -A myapp
```

## Future Improvements

1. **Header Dependencies**: Implement compiler-based scanning (-MM flags)
2. **External Libraries**: Comprehensive find_package() mapping
3. **Cross-Compilation**: Multi-platform support
4. **Performance**: Optimize large project generation
5. **Nix Integration**: Leverage more Nix features (overlays, flakes)