# Issue #010: CMake Self-Hosting Validation

**Priority:** HIGH  
**Status:** Not Started  
**Category:** Validation  
**Estimated Effort:** 1-2 weeks

## Problem Description

The ultimate test of the Nix generator is whether it can build CMake itself. This would validate:
- Scalability to large projects
- Handling of complex dependencies
- Real-world feature usage
- Performance characteristics

## Current Status

CMake is a large C++ project with:
- 600+ source files
- Complex dependency graph
- Custom commands for code generation
- External library dependencies
- Multiple target types

The Nix generator is not yet capable of building CMake.

## Expected Behavior

The generator should:
1. Successfully generate Nix expressions for CMake
2. Build a working CMake executable
3. Pass CMake's test suite
4. Perform reasonably compared to other generators

## Gap Analysis

### Current Blockers

1. **C++ Support** - Currently only C is fully tested
2. **find_package() Support** - CMake uses many external libraries
3. **Custom Commands** - CMake generates code during build
4. **Complex Target Dependencies** - Inter-target dependencies
5. **Install Rules** - CMake has complex installation

### Required Features

- [ ] Full C++ compilation support
- [ ] All standard find_package() modules
- [ ] Custom command execution
- [ ] Target dependency resolution
- [ ] Install rule processing
- [ ] Multi-directory projects
- [ ] Generated header dependencies

## Implementation Plan

### Phase 1: Basic CMake Build
```bash
# Initial attempt
cd CMake
mkdir build-nix
cd build-nix
cmake -G Nix -DCMAKE_MAKE_PROGRAM=nix-build ..

# Expected issues:
# - Missing C++ support
# - External library failures
# - Custom command failures
```

### Phase 2: Address Compilation Issues

#### C++ Compiler Flags
```cpp
// Add to cmNixTargetGenerator
std::string GetCompileFlags(cmSourceFile* source) {
  std::string flags;
  std::string lang = source->GetLanguage();
  
  if (lang == "CXX") {
    // C++ specific flags
    flags += "-std=c++17 ";  // CMake requires C++17
    flags += this->GetCxxFlags();
  }
  
  return flags;
}
```

#### External Libraries
CMake's dependencies:
- libcurl (optional)
- libarchive (optional)
- expat or jsoncpp
- zlib
- libuv
- rhash

Mapping needed:
```cpp
{"CURL", "curl.dev"},
{"LibArchive", "libarchive.dev"},
{"EXPAT", "expat.dev"},
{"ZLIB", "zlib.dev"},
{"LibUV", "libuv.dev"},
{"RHash", "rhash"}
```

### Phase 3: Fix Custom Commands

CMake generates several files:
```cmake
# Source/CMakeVersionSource.cmake
configure_file(
  ${CMake_SOURCE_DIR}/Source/CMakeVersion.cmake.in
  ${CMake_BINARY_DIR}/Source/CMakeVersion.cmake @ONLY
)

# Parser generation
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/cmExprParser.cxx
  COMMAND ${BISON_EXECUTABLE}
  # ...
)
```

### Phase 4: Performance Optimization

Expected issues with 600+ files:
- Nix evaluation time
- Parallelism bottlenecks
- Memory usage

Solutions:
- Implement batched compilation
- Optimize Nix expressions
- Profile and tune

### Phase 5: Test Suite Validation

```bash
# After successful build
cd build-nix
ctest -j8

# Compare with Makefiles build
cd ../build-make
time make -j8
cd ../build-nix
time nix-build
```

## Validation Checklist

### Build Validation
- [ ] CMake configures with Nix generator
- [ ] All source files compile
- [ ] All targets link successfully
- [ ] cmake executable runs
- [ ] ccmake, ctest, cpack build

### Feature Validation
- [ ] Can run cmake --version
- [ ] Can configure a simple project
- [ ] Can configure CMake itself (self-hosting)
- [ ] Bootstrap works with Nix-built CMake

### Test Suite
- [ ] RunCMake tests pass
- [ ] Module tests pass
- [ ] Command tests pass
- [ ] At least 95% test pass rate

### Performance Metrics
| Metric | Target | Actual |
|--------|--------|--------|
| Generation time | < 10s | TBD |
| Full build time | < 3x Makefiles | TBD |
| Incremental build | < 2x Makefiles | TBD |
| Nix evaluation | < 5s | TBD |
| Memory usage | < 2GB | TBD |

## Known Challenges

### 1. Bootstrap Process
CMake can bootstrap itself without CMake. Need to ensure Nix generator doesn't break this.

### 2. Platform-Specific Code
CMake has platform-specific code. Initially focus on Linux.

### 3. Optional Features
Start with minimal configuration, add features incrementally.

### 4. Build Performance
600+ individual derivations may be slow. Consider batching strategy.

## Success Criteria

- [ ] CMake builds successfully with Nix generator
- [ ] Built CMake can configure projects
- [ ] Performance is within acceptable range
- [ ] Test suite passes (>95%)
- [ ] Can self-host (build CMake with Nix-built CMake)

## Benefits of Success

1. **Validation** - Proves generator handles real-world complexity
2. **Dogfooding** - CMake team can use Nix generator
3. **Performance** - Real benchmark for optimization
4. **Confidence** - If it can build CMake, it can build anything

## Incremental Milestones

1. **Milestone 1**: Generate valid default.nix for CMake
2. **Milestone 2**: Compile 50% of source files
3. **Milestone 3**: All files compile, linking fails
4. **Milestone 4**: Successful build, some tests fail
5. **Milestone 5**: Full test suite passes

## Related Files

- `Source/cmGlobalNixGenerator.cxx` - Main generator
- `Source/cmNixTargetGenerator.cxx` - Target generation
- CMake's own CMakeLists.txt files for test cases