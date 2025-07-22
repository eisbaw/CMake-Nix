# Unsupported Features in CMake Nix Backend

Based on investigation of the CMake Nix backend implementation, the following features are currently unsupported:

## 1. Target Types

### OBJECT Libraries
- `add_library(name OBJECT ...)` creates object libraries but they are not compiled
- `$<TARGET_OBJECTS:name>` generator expression is not evaluated
- Object files from OBJECT libraries are not included in targets that reference them

### MODULE Libraries  
- `add_library(name MODULE ...)` creates plugin/module libraries
- MODULE libraries are completely ignored - no derivations are generated for them
- They are not included in the checks for supported target types

## 2. Target Properties
The following commonly used target properties appear to have no handling:
- `OUTPUT_NAME` - custom output names for targets
- `PREFIX` - custom prefix for libraries (e.g., removing "lib")
- `SUFFIX` - custom suffix for libraries
- `RUNTIME_OUTPUT_DIRECTORY` - custom output directory for executables
- `LIBRARY_OUTPUT_DIRECTORY` - custom output directory for libraries
- `ARCHIVE_OUTPUT_DIRECTORY` - custom output directory for static libraries

## 3. Build Features
- Pre/Post build commands are collected but not processed:
  - `add_custom_command(TARGET ... PRE_BUILD ...)`
  - `add_custom_command(TARGET ... PRE_LINK ...)`  
  - `add_custom_command(TARGET ... POST_BUILD ...)`

## 4. TODO Comments in Code
From the source code TODOs:
- Header dependency tracking is marked as TODO
- Some derivation writing is marked as TODO
- Include flags for Nix derivations marked as TODO
- Clang-tidy replacements file path marked as TODO

## Recommendations

1. **OBJECT library support** is critical as it's commonly used for code organization
2. **MODULE library support** is important for plugin architectures
3. **Target property support** for output names/paths is important for compatibility
4. **Pre/Post build commands** are commonly used for code generation and packaging

These features should be prioritized based on how commonly they are used in real CMake projects.