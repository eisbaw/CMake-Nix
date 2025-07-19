# Issue #011: Documentation and User Guide

**Priority:** MEDIUM  
**Status:** Not Started  
**Category:** Documentation  
**Estimated Effort:** 2-3 days

## Problem Description

While we have basic documentation (README.md, PRD.md), we need comprehensive user-facing documentation that:
- Explains how to use the Nix generator
- Provides examples and best practices
- Documents limitations and workarounds
- Integrates with CMake's documentation system

## Current Documentation

Existing:
- README.md - Basic overview and usage
- PRD.md - Technical requirements
- Help/generator/Nix.rst - Minimal generator docs

Missing:
- Detailed user guide
- Migration guide from other generators
- Troubleshooting guide
- Example projects
- API documentation

## Expected Documentation

### 1. User Guide Structure
```
1. Introduction
   - What is the Nix generator?
   - Benefits over traditional generators
   - When to use it

2. Getting Started
   - Prerequisites
   - Basic usage
   - First project

3. Features
   - Fine-grained caching
   - Reproducible builds
   - Distributed compilation
   - Nix integration

4. Configuration
   - Generator options
   - Performance tuning
   - Custom mappings

5. Examples
   - Simple executable
   - Library project
   - External dependencies
   - Custom commands

6. Troubleshooting
   - Common issues
   - Debug techniques
   - Performance tips

7. Reference
   - Supported features
   - Limitations
   - Option reference
```

## Implementation Plan

### 1. Enhance Help/generator/Nix.rst
```rst
Nix
---

Generates Nix expressions for building CMake projects.

The ``Nix`` generator produces a ``default.nix`` file that describes
how to build the project using the Nix package manager. Unlike traditional
generators that produce makefiles or build scripts, the Nix generator
creates functional package definitions with fine-grained dependency tracking.

Features
^^^^^^^^

* **Fine-grained caching**: Each source file is compiled in its own derivation
* **Reproducible builds**: All builds happen in isolated environments
* **Distributed builds**: Leverage Nix's remote build capabilities
* **Content-addressed storage**: Automatic deduplication of build artifacts

Usage
^^^^^

Generate build files::

  cmake -G Nix -DCMAKE_MAKE_PROGRAM=nix-build /path/to/source

Build the project::

  nix-build -A myapp

Build specific configuration::

  nix-build -A myapp_debug

Options
^^^^^^^

``CMAKE_NIX_BATCH_COMPILATION``
  Enable batched compilation for better performance with many small files.
  
``CMAKE_NIX_BATCH_SIZE``
  Number of files to compile per batch (default: 10).

``CMAKE_NIX_METRICS``
  Enable build metrics collection.

Limitations
^^^^^^^^^^^

* No support for cross-compilation (yet)
* Custom commands have restrictions due to Nix sandbox
* Windows support is experimental

Example
^^^^^^^

.. code-block:: cmake

   cmake_minimum_required(VERSION 3.20)
   project(MyApp C CXX)
   
   find_package(ZLIB REQUIRED)
   
   add_executable(myapp
     src/main.cpp
     src/util.cpp
   )
   
   target_link_libraries(myapp PRIVATE ZLIB::ZLIB)
   
   install(TARGETS myapp DESTINATION bin)

This generates a Nix expression that builds ``myapp`` with proper
dependency tracking and caching.
```

### 2. Create Examples Directory
```
Help/examples/nix-generator/
  01-simple-executable/
    CMakeLists.txt
    main.c
    README.md
  
  02-static-library/
    CMakeLists.txt
    src/lib.c
    include/lib.h
    README.md
  
  03-external-dependencies/
    CMakeLists.txt
    compress.c
    README.md
  
  04-custom-commands/
    CMakeLists.txt
    generate.py
    main.c
    README.md
```

### 3. Migration Guide
```markdown
# Migrating to the Nix Generator

## From Unix Makefiles

The Nix generator provides similar functionality with added benefits:

### Before (Makefiles)
```bash
cmake -G "Unix Makefiles" ..
make -j8
```

### After (Nix)
```bash
cmake -G Nix -DCMAKE_MAKE_PROGRAM=nix-build ..
nix-build -A myapp
```

## Key Differences

1. **Build Command**: Use `nix-build` instead of `make`
2. **Parallelism**: Automatic, no need for `-j` flag
3. **Output**: Results in `result` symlink
4. **Caching**: Automatic content-addressed caching

## Common Adjustments

### Custom Commands
Nix runs in a sandbox, so:
- No network access during build
- No absolute paths outside build directory
- Generated files must be declared as outputs

### External Tools
Tools must be in buildInputs:
```cmake
# May need to specify tool availability
find_program(PYTHON python3 REQUIRED)
```
```

### 4. Troubleshooting Guide
```markdown
# Troubleshooting

## Build Failures

### "attribute 'X' missing"
The target X was not generated. Check:
- Target name spelling
- Target type is supported
- No CMake configuration errors

### "cannot execute binary file"
Cross-compilation issue. Ensure:
- Correct compiler selected
- Build/host platforms match

### Performance Issues

#### Slow Evaluation
Too many derivations. Try:
```cmake
set(CMAKE_NIX_BATCH_COMPILATION ON)
set(CMAKE_NIX_BATCH_SIZE 20)
```

#### Excessive Rebuilds
Check header dependencies:
```bash
nix-store --query --tree result
```

## Debug Techniques

### Verbose Output
```bash
nix-build -A myapp --verbose
```

### Examine Derivation
```bash
nix show-derivation ./default.nix -A myapp
```

### Build Single Object
```bash
nix-build -A myapp_main_c_o
```
```

### 5. Integration with CMake Docs
```cmake
# In CMake's Documentation build
add_custom_target(documentation-nix-generator
  COMMAND ${SPHINX_EXECUTABLE}
    -b html
    ${CMAKE_CURRENT_SOURCE_DIR}/Help
    ${CMAKE_CURRENT_BINARY_DIR}/Help/html
  DEPENDS
    Help/generator/Nix.rst
    Help/examples/nix-generator/*
)
```

## Acceptance Criteria

- [ ] Comprehensive generator documentation in Help/generator/Nix.rst
- [ ] At least 5 example projects with README files
- [ ] Migration guide covering common scenarios
- [ ] Troubleshooting guide with solutions
- [ ] Documentation builds with CMake's docs
- [ ] Examples are tested and working
- [ ] Cross-references to Nix documentation

## Documentation Standards

Follow CMake's documentation guidelines:
- Use RST format for Help/ files
- Include practical examples
- Document all options
- Explain limitations clearly
- Provide troubleshooting tips

## Future Documentation

- Video tutorials
- Blog post announcements
- Conference presentation materials
- Comparison with other generators
- Advanced usage patterns

## Related Files

- `Help/generator/Nix.rst` - Main documentation
- `Help/manual/cmake-generators.7.rst` - Add Nix to list
- `Help/examples/nix-generator/` - Example projects
- `README.md` - Update with link to full docs