# Issue #009: Comprehensive Test Suite

**Priority:** HIGH  
**Status:** Not Started  
**Category:** Testing & Validation  
**Estimated Effort:** 3-4 days

## Problem Description

While we have basic test projects, we need a comprehensive test suite that:
- Validates all features systematically
- Catches regressions
- Provides examples for users
- Integrates with CMake's test infrastructure

## Current Test Coverage

Existing tests:
- test_multifile/ - Basic multi-file compilation
- test_headers/ - Include directory support
- test_implicit_headers/ - Header dependencies
- test_explicit_headers/ - Manual dependencies
- test_external_library/ - External library linking
- test_zlib_example/ - Real library example

Missing coverage:
- CMake test integration
- Automated validation
- Performance benchmarks
- Error handling tests

## Expected Behavior

The test suite should:
1. Run automatically via CTest
2. Validate generated Nix expressions
3. Build and run test projects
4. Check for correctness
5. Report clear failures

## Implementation Plan

### 1. Create Test Infrastructure
```cmake
# Tests/RunCMake/NixGenerator/CMakeLists.txt
include(RunCMake)

# Basic generation tests
run_cmake(SimpleExecutable)
run_cmake(StaticLibrary)
run_cmake(SharedLibrary)
run_cmake(MultipleTargets)

# Feature tests
run_cmake(IncludeDirectories)
run_cmake(CompileDefinitions)
run_cmake(ConfigurationTypes)
run_cmake(CustomCommands)

# Error cases
run_cmake(MissingSource-result-check)
run_cmake(InvalidTarget-result-check)
```

### 2. Test Case Structure
```
Tests/RunCMake/NixGenerator/
  SimpleExecutable/
    CMakeLists.txt
    main.c
    RunCMakeTest.cmake
    SimpleExecutable-check.cmake
  
  StaticLibrary/
    CMakeLists.txt
    lib.c
    lib.h
    RunCMakeTest.cmake
    StaticLibrary-check.cmake
```

### 3. Validation Scripts
```cmake
# SimpleExecutable-check.cmake
set(nix_file "${RunCMake_TEST_BINARY_DIR}/default.nix")

# Check file exists
if(NOT EXISTS "${nix_file}")
  set(RunCMake_TEST_FAILED "default.nix not generated")
  return()
endif()

# Validate Nix syntax
execute_process(
  COMMAND nix-instantiate --parse "${nix_file}"
  RESULT_VARIABLE nix_parse_result
  ERROR_VARIABLE nix_parse_error
)

if(NOT nix_parse_result EQUAL 0)
  set(RunCMake_TEST_FAILED "Invalid Nix syntax: ${nix_parse_error}")
  return()
endif()

# Check for expected derivations
file(READ "${nix_file}" nix_content)
if(NOT nix_content MATCHES "simple_exe = stdenv\\.mkDerivation")
  set(RunCMake_TEST_FAILED "Missing executable derivation")
  return()
endif()
```

### 4. Build and Run Tests
```cmake
# NixBuildTest.cmake
function(add_nix_build_test name)
  add_test(
    NAME NixGenerator.${name}.generate
    COMMAND ${CMAKE_COMMAND}
      -G Nix
      -DCMAKE_MAKE_PROGRAM=nix-build
      -S ${CMAKE_CURRENT_SOURCE_DIR}/${name}
      -B ${CMAKE_CURRENT_BINARY_DIR}/${name}
  )
  
  add_test(
    NAME NixGenerator.${name}.build
    COMMAND nix-build
      --no-out-link
      ${CMAKE_CURRENT_BINARY_DIR}/${name}/default.nix
      -A default
  )
  
  set_tests_properties(
    NixGenerator.${name}.build
    PROPERTIES DEPENDS NixGenerator.${name}.generate
  )
  
  # If executable, add run test
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${name}/expected_output.txt)
    add_test(
      NAME NixGenerator.${name}.run
      COMMAND ${CMAKE_COMMAND} -E compare_files
        ${CMAKE_CURRENT_SOURCE_DIR}/${name}/expected_output.txt
        ${CMAKE_CURRENT_BINARY_DIR}/${name}/actual_output.txt
    )
    set_tests_properties(
      NixGenerator.${name}.run
      PROPERTIES DEPENDS NixGenerator.${name}.build
    )
  endif()
endfunction()
```

### 5. Feature-Specific Tests

#### Header Dependency Test
```cmake
# HeaderDependency/CMakeLists.txt
project(HeaderDependencyTest C)

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/config.h" 
  "#define VERSION \"1.0\"\n")

add_executable(version_test main.c)
target_include_directories(version_test PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}
)

# Test validation checks that:
# 1. config.h is included in derivation inputs
# 2. Rebuilds when config.h changes
```

#### External Library Test
```cmake
# ExternalLibrary/CMakeLists.txt
find_package(ZLIB REQUIRED)
add_executable(compress_test compress.c)
target_link_libraries(compress_test ZLIB::ZLIB)

# Validation:
# - Check zlib in buildInputs
# - Verify executable runs and compresses data
```

#### Custom Command Test
```cmake
# CustomCommand/CMakeLists.txt
add_custom_command(
  OUTPUT generated.h
  COMMAND echo "#define GENERATED 1" > generated.h
  VERBATIM
)

add_executable(custom_test main.c generated.h)

# Validation:
# - Custom command derivation exists
# - Generated file is created
# - Main executable depends on custom command
```

### 6. Performance Benchmarks
```cmake
# Benchmark/CMakeLists.txt
# Large project with 100+ source files

add_test(
  NAME NixGenerator.Benchmark.time
  COMMAND ${CMAKE_COMMAND} -E time
    ${CMAKE_COMMAND} -G Nix ${CMAKE_CURRENT_SOURCE_DIR}/LargeProject
)

# Compare with Makefiles generator
add_test(
  NAME Makefiles.Benchmark.time
  COMMAND ${CMAKE_COMMAND} -E time
    ${CMAKE_COMMAND} -G "Unix Makefiles" ${CMAKE_CURRENT_SOURCE_DIR}/LargeProject
)
```

### 7. Error Handling Tests
```cmake
# MissingSource/CMakeLists.txt
add_executable(broken nonexistent.c)

# Expected: Graceful error message

# CircularDependency/CMakeLists.txt
# Create circular dependency scenario
# Expected: Detection and error
```

## Test Matrix

| Feature | Unit Test | Integration Test | Build Test | Run Test |
|---------|-----------|-----------------|------------|----------|
| Simple executable | ✓ | ✓ | ✓ | ✓ |
| Static library | ✓ | ✓ | ✓ | - |
| Shared library | ✓ | ✓ | ✓ | ✓ |
| Include directories | ✓ | ✓ | ✓ | ✓ |
| Compile flags | ✓ | ✓ | ✓ | ✓ |
| Link libraries | ✓ | ✓ | ✓ | ✓ |
| External libraries | ✓ | ✓ | ✓ | ✓ |
| Custom commands | ✓ | ✓ | ✓ | ✓ |
| Subdirectories | ✓ | ✓ | ✓ | ✓ |
| Install rules | ✓ | ✓ | ✓ | - |

## Acceptance Criteria

- [ ] All tests run via `ctest -R NixGenerator`
- [ ] Test failures provide clear error messages
- [ ] Coverage of all major features
- [ ] Performance benchmarks available
- [ ] Error cases handled gracefully
- [ ] Tests run in CI environment
- [ ] Documentation for adding new tests

## CI Integration

```yaml
# .github/workflows/nix-generator-tests.yml
- name: Run Nix Generator Tests
  run: |
    cd build
    ctest -R NixGenerator --output-on-failure
```

## Related Files

- `Tests/RunCMake/NixGenerator/` - Main test directory
- `Tests/CMakeLists.txt` - Test registration
- Existing test projects for migration