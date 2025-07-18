# CMake Nix Generator Implementation Plan

## Current Status

### ✅ Phase 1: Complete
- Basic generator infrastructure implemented
- Per-translation-unit derivations working
- Simple C/C++ project support
- Clean object naming with relative paths

### ✅ Phase 2: Core Features Complete (75%)
**Major Achievements:**
- ✅ **Source file handling fixed** - Nix derivations properly unpack and access source files
- ✅ **Include path support** - Multi-directory projects with headers work (`-Iinclude`)
- ✅ **Build configuration support** - Debug/Release detection with preprocessor definitions
- ✅ **Basic target types** - Executables and static libraries supported

**Successfully Tested:**
- ✅ Simple multi-file projects (calculator example)
- ✅ Complex projects with headers in separate directories  
- ✅ Configuration-specific builds (Debug vs Release)
- ✅ Preprocessor definitions and include paths working together

## Critical Gaps Requiring Immediate Attention

### 1. Header Dependency Tracking (Critical - Correctness Issue)
**Problem**: `GetSourceDependencies()` returns empty - silent correctness bugs
**Impact**: Header changes don't trigger rebuilds
**Root Cause**: `cmDependsC` requires `cmLocalUnixMakefileGenerator3`, incompatible with `cmLocalNixGenerator`

**Solution Decision: Option B - Compiler-Based Dependency Scanning**
After architectural analysis, Option B is the most robust and correct-by-design approach:
- ✅ **Compiler authoritative**: Uses definitive source of dependency truth
- ✅ **Industry standard**: Same approach as Ninja, Bazel, modern build systems  
- ✅ **Architecturally sound**: Self-contained, no coupling to other generators
- ✅ **Future-proof**: Independent of CMake generator internals
- ✅ **Nix-aligned**: Explicit dependencies match Nix philosophy

**Implementation Strategy:**
1. **Phase 1**: Basic `-MM`/`-MD` support for GCC/Clang (covers 95% of use cases)
2. **Phase 2**: Fallback to CMake's regex-based scanner for legacy compilers
3. **Phase 3**: Manual dependency specification support for edge cases

**Compiler Support Matrix:**
- ✅ **GCC**: `-MM`, `-MD -MF` (primary target)
- ✅ **Clang**: `-MM`, `-MD -MF` (primary target)  
- ✅ **MSVC**: `/showIncludes` (Phase 2)
- ✅ **Legacy/Custom**: Regex scanner fallback (Phase 2)
- ✅ **Manual**: `OBJECT_DEPENDS` property support (Phase 3)

**Fallback Strategy for Compilers Without Dependency Flags:**
CMake provides several mechanisms for compilers that don't support automatic dependency generation:
1. **Built-in regex scanner**: CMake's `cmDependsC::Scan()` for basic `#include` parsing
2. **Manual specification**: `set_source_files_properties(file.c PROPERTIES OBJECT_DEPENDS "header1.h;header2.h")`
3. **Directory-level**: `include_directories()` creates implicit dependencies
4. **Target-level**: Interface dependencies via `target_link_libraries()` with header-only targets

**Priority**: **HIGH** - Blocks correctness for real projects

### 2. External Library Support (Major - Functionality Gap)
**Problem**: No support for system libraries, find_package(), pkg-config
**Impact**: Can't build projects with dependencies (most real projects)

**Examples that fail:**
```cmake
find_package(OpenGL REQUIRED)
target_link_libraries(myapp OpenGL::GL)
```

**Solution**: Map CMake imported targets to Nix packages
**Priority**: **HIGH** - Blocks most real-world projects

### 3. Compiler Auto-Detection (Minor - In Progress)
**Problem**: Hardcoded to `gcc`
**Impact**: Can't use clang, cross-compilation, or detect optimal compiler
**Solution**: Detect compiler from CMake variables, generate appropriate buildInputs
**Priority**: **MEDIUM**

## Implementation Roadmap

### Phase 2 Completion (Target: 2-3 weeks)

#### Week 1: Critical Fixes
1. **Header Dependency Tracking** 
   - Implement compiler-based dependency scanning (Option B)
   - Start with GCC/Clang `-MM` support (covers majority of users)
   - Add fallback to CMake's regex scanner for legacy compilers
   - Test with projects that have complex header dependencies

2. **Basic External Library Support**
   - Implement mapping for common system libraries (OpenGL, pthread, math)
   - Add support for imported targets in link derivations
   - Test with projects using find_package()

#### Week 2: Enhanced Features  
3. **Compiler Auto-Detection**
   - Detect compiler from CMAKE_C_COMPILER, CMAKE_CXX_COMPILER
   - Generate appropriate Nix buildInputs (gcc, clang, etc.)
   - Support basic cross-compilation scenarios

4. **Shared Library Support**
   - Implement shared library target generation
   - Handle library versioning and RPATH
   - Test with mixed static/shared library projects

#### Week 3: Validation & Polish
5. **Complex Project Testing**
   - Test with medium-complexity open source projects
   - Test with CMake itself (ultimate validation)
   - Fix issues discovered during real-world testing

### Phase 3: Production Features (Target: 4-6 weeks)

#### Advanced Target Support
- Custom commands and code generation
- Subdirectory support with multiple CMakeLists.txt
- Install rules and packaging
- Multi-configuration generator support

#### Ecosystem Integration  
- Advanced find_package() support
- pkg-config integration
- Nix package ecosystem integration
- Performance optimizations

#### Enterprise Features
- Large codebase support
- Incremental builds optimization
- Build caching strategies
- CI/CD integration

## Testing Strategy

### Current Test Coverage
- ✅ Simple multi-file projects
- ✅ Projects with include directories
- ✅ Basic configuration testing

### Immediate Test Additions Needed
1. **Header dependency validation** - Projects where header changes should trigger rebuilds
2. **External library projects** - Basic OpenGL or pthread examples
3. **Mixed target projects** - Executables linking against libraries
4. **Configuration-specific flags** - Verify Debug vs Release actually produces different flags

### Phase 3 Test Targets
- **CMake itself** - Ultimate complexity test
- **Popular open source projects** - Real-world validation
- **Performance benchmarks** - Build time comparisons

## Success Metrics

### Phase 2 Completion Criteria
- [ ] Header dependency changes trigger correct rebuilds
- [ ] Projects with basic external dependencies build successfully  
- [ ] Shared libraries can be built and linked
- [ ] CMake's own test suite passes with Nix generator (subset)

### Phase 3 Completion Criteria
- [ ] Feature parity with Unix Makefiles generator for common use cases
- [ ] Performance comparable to other generators
- [ ] Integration with Nix ecosystem tools
- [ ] Real-world projects build successfully

## Risk Assessment & Mitigation

### Technical Risks
1. **Header dependency complexity** - CMake's dependency system is intricate
   - *Mitigation*: Start with compiler-based approach, iterate
2. **External library mapping** - Nix package ecosystem is vast
   - *Mitigation*: Focus on most common libraries first
3. **Performance concerns** - Fine-grained derivations may be slow
   - *Mitigation*: Profile and optimize, consider build parallelism

### Scope Risks  
1. **Feature creep** - CMake has enormous surface area
   - *Mitigation*: Strict prioritization, focus on 80/20 rule
2. **Nix ecosystem changes** - Dependencies on external Nix infrastructure
   - *Mitigation*: Use stable Nix APIs, document dependencies

## Next Actions

### Immediate (This Week)
1. **Research header dependency solutions** - Deep dive into CMake's dependency infrastructure
2. **Implement compiler-based dependency scanning** - Use `gcc -M` approach
3. **Create external library mapping framework** - Start with common system libraries

### Short-term (Next 2 weeks)  
4. **Test with real projects** - Find suitable medium-complexity test cases
5. **Implement shared library support** - Critical for many projects
6. **Performance testing** - Ensure scalability

The goal is to move from "proof of concept" to "production ready for moderate complexity projects" by completing Phase 2, with a clear path to full production readiness in Phase 3.