# CMake Nix Backend

A CMake generator that produces Nix expressions for building C/C++ projects with fine-grained derivations.

## Overview

This project implements a new CMake generator backend that outputs Nix expressions instead of traditional makefiles or build.ninja files. The generator creates one Nix derivation per translation unit, enabling maximal build parallelism and leveraging Nix's advanced caching capabilities.

## Current Status

**Phase 1 Complete** - Basic functionality working:

- ✅ Generator registration and CMake integration
- ✅ Per-translation-unit derivation generation  
- ✅ Support for executables and static libraries
- ✅ Valid Nix expressions that build with `nix-build`
- ✅ Multi-file projects with separate compilation

**Tested and Working:**
- Simple hello world programs
- Multi-file projects with multiple targets
- Both executable and static library targets

## Usage

### Configuration
```bash
# Configure project to use Nix generator
cmake -G Nix -DCMAKE_MAKE_PROGRAM=nix-build /path/to/source
```

### Building
```bash
# Build specific target
nix-build -A target_name

# Run the result
./result
```

## Generated Structure

For a project with `main.c`, the generator creates `default.nix`:

```nix
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
```

## Implementation Files

### Core Generator Classes
- `Source/cmGlobalNixGenerator.{h,cxx}` - Main generator implementation
- `Source/cmLocalNixGenerator.{h,cxx}` - Local generator for per-directory handling  
- `Source/cmNixTargetGenerator.{h,cxx}` - Per-target derivation generation
- `Modules/CMakeNixFindMake.cmake` - Build tool discovery module

### Documentation
- `Help/generator/Nix.rst` - User-facing generator documentation
- `PRD.md` - Product Requirements Document with full specification

## Benefits

### Fine-Grained Parallelism
Each source file compiles independently, allowing Nix to maximize parallel execution across available cores and even distributed build farms.

### Advanced Caching
Nix's content-addressed storage means object files are cached permanently. Changing one source file only recompiles that file and relinks affected targets.

### Reproducible Builds
All builds happen in hermetic environments with precisely specified dependencies, ensuring identical results across different machines.

### Distributed Builds
Integration with Nix's remote build capabilities enables transparent distributed compilation.

## Current Limitations

- **Header Dependencies**: Not yet tracked, compilation fails when headers are included
- **Include Paths**: Not passed to gcc during compilation  
- **Single Configuration**: Only default configuration supported
- **External Libraries**: No integration with system packages yet

## Next Steps

See "Next Steps" section below for planned Phase 2 enhancements.

## Testing

Two test projects demonstrate current functionality:

### Simple Test
```bash
cd test_nix_generator
cmake -G Nix -DCMAKE_MAKE_PROGRAM=nix-build .
nix-build -A hello
./result  # Outputs: Hello, Nix world!
```

### Multi-File Test  
```bash
cd test_multifile
cmake -G Nix -DCMAKE_MAKE_PROGRAM=nix-build .
nix-build -A calculator  
./result  # Outputs: 2 + 3 = 5, 5 - 2 = 3
```

## Contributing

This implementation follows CMake's established generator architecture patterns. See `PRD.md` for detailed technical specifications and planned features.