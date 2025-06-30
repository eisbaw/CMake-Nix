# Product Requirements Document: CMake Nix Backend

## Overview

This PRD specifies the development of a new CMake generator backend that produces Nix expressions for building C/C++ projects. The Nix backend will enable CMake to generate fine-grained Nix derivations that maximize build parallelism and caching efficiency.

## Goals

### Primary Goal
Enable CMake to generate Nix expressions that can build C/C++ projects with maximal reuse and fine-grained dependency tracking.

### Secondary Goals
- Leverage Nix's caching and reproducibility features
- Enable distributed builds through Nix's remote build capabilities
- Provide a foundation for hermetic builds in Nix environments

## Architecture Overview

### Backend Implementation
The Nix backend will follow CMake's existing generator architecture:

1. **cmGlobalNixGenerator** - Inherits from `cmGlobalCommonGenerator`
2. **cmLocalNixGenerator** - Inherits from `cmLocalGenerator`  
3. **cmNixTargetGenerator** - Handles per-target Nix derivation generation

### Key Files Location
Based on analysis of existing backends:
- Main generator: `CMake/Source/cmGlobalNixGenerator.{h,cxx}`
- Local generator: `CMake/Source/cmLocalNixGenerator.{h,cxx}`
- Target generator: `CMake/Source/cmNixTargetGenerator.{h,cxx}`
- Documentation: `CMake/Help/generator/Nix.rst`

## Technical Requirements

### Phase 1: Core Implementation
**Scope**: Basic C compilation with GCC, single-configuration builds

#### 1.1 Generator Registration
- Add `cmGlobalNixGenerator::NewFactory()` to `cmake.cxx` generator registration
- Implement factory pattern following `cmGlobalGeneratorSimpleFactory<T>`
- Generator name: "Nix"

#### 1.2 Fine-Grained Derivations
Generate one Nix derivation per translation unit:
```nix
{
  # Per-source-file derivation
  "src/main.c.o" = stdenv.mkDerivation {
    name = "main.c.o";
    src = ./src/main.c;
    buildInputs = [ gcc ];
    # Only include headers this translation unit actually depends on
    dependencies = [ ./include/header1.h ./include/header2.h ];
    buildPhase = "gcc -c src/main.c -o $out";
  };
  
  # Linking derivation that depends on object derivations
  "myproject" = stdenv.mkDerivation {
    name = "myproject";
    buildInputs = [ gcc ];
    dependencies = [ self."src/main.c.o" self."src/other.c.o" ];
    buildPhase = "gcc $dependencies -o $out";
  };
}
```

#### 1.3 Dependency Tracking
Leverage CMake's existing dependency analysis:
- Use `cmGeneratorTarget::GetSourceFiles()` to enumerate translation units
- Extract header dependencies from CMake's dependency scanner
- Map CMake's internal dependency graph to Nix derivation dependencies

#### 1.4 Output Structure
Generate single `default.nix` file containing:
- All per-translation-unit derivations
- Linking derivations for executables/libraries
- Top-level derivation exposing final targets

### Phase 2: Enhanced Features
**Scope**: Header dependency tracking, include paths, multi-configuration support

#### 2.1 Header Dependency Tracking (Critical)
**Status**: Required for complex projects - current limitation prevents building projects with multiple source files that include headers.

- Implement header dependency scanning using CMake's `cmDepends` infrastructure
- Add header files as derivation inputs to ensure proper rebuilds
- Track transitive header dependencies across translation units
- Handle system headers vs project headers appropriately

**Implementation Details**:
```cpp
// In cmNixTargetGenerator::WriteObjectDerivation()
std::vector<std::string> GetHeaderDependencies(cmSourceFile* source) {
  // Use cmDepends to scan for #include directives
  // Map to actual header file paths
  // Return list of header dependencies for this source file
}
```

**Example Enhanced Derivation**:
```nix
myproject_main_c_o = stdenv.mkDerivation {
  name = "main.c.o";
  src = ./.;
  buildInputs = [ gcc ];
  # Headers this translation unit depends on
  inputs = [ ./src/calculator.h ./include/common.h ];
  buildPhase = "gcc -c src/main.c -I./include -o $out";
};
```

#### 2.2 Include Path Support (Critical)  
**Status**: Required for multi-directory projects - current implementation cannot handle projects with headers in separate directories.

- Pass target-specific include directories as `-I` flags to compilation derivations
- Support both `target_include_directories()` and global include paths
- Handle relative vs absolute include paths correctly
- Support interface include directories for library targets

**Implementation Details**:
```cpp
// Generate proper compiler flags with include paths
std::string GetCompileFlags(cmGeneratorTarget* target, cmSourceFile* source) {
  std::vector<std::string> includes = target->GetIncludeDirectories();
  // Convert to -I flags for gcc/clang
  // Add language-specific flags
  // Handle configuration-specific includes
}
```

#### 2.3 Build Configuration Support
- Generate per-configuration derivation sets (Debug/Release/etc.)
- Support configuration-specific compiler flags and optimization levels
- Handle preprocessor definitions (`-D` flags)
- Implement `IsMultiConfig()` to return `true` for multi-config generators

**Configuration-Specific Derivations**:
```nix
# Debug configuration
myproject_debug = stdenv.mkDerivation {
  name = "myproject-debug";
  buildInputs = [ gcc gdb ];
  buildPhase = "gcc -g -O0 -DDEBUG $inputs -o $out";
};

# Release configuration  
myproject_release = stdenv.mkDerivation {
  name = "myproject-release";
  buildInputs = [ gcc ];
  buildPhase = "gcc -O2 -DNDEBUG $inputs -o $out";
};
```

#### 2.4 Compiler Auto-Detection
- Detect available compilers (gcc, clang, etc.)
- Generate appropriate Nix buildInputs based on detected compiler
- Handle cross-compilation scenarios
- Support compiler-specific flags and features

#### 2.5 Advanced Dependencies
- External library dependencies through Nix packages
- pkg-config integration for system libraries
- CMake find_package() mapping to Nix packages
- Support for imported targets and interface libraries

### Phase 3: Production Features
**Scope**: Full feature parity with other backends

#### 3.1 Custom Commands
- Support for custom build steps
- Pre/post-build commands
- Code generation steps

#### 3.2 Install Rules
- Generate Nix install phases
- Handle file permissions and destinations
- Support component-based installation

## Implementation Details

### Core Classes Structure

```cpp
class cmGlobalNixGenerator : public cmGlobalCommonGenerator
{
public:
  static std::unique_ptr<cmGlobalGeneratorFactory> NewFactory();
  std::string GetName() const override { return "Nix"; }
  static std::string GetActualName() { return "Nix"; }
  
  void Generate() override;
  std::unique_ptr<cmLocalGenerator> CreateLocalGenerator(cmMakefile* mf) override;
  
private:
  void WriteNixFile();
  void WriteDerivations();
};

class cmLocalNixGenerator : public cmLocalGenerator
{
public:
  void Generate() override;
  void GenerateTargetManifest() override;
};

class cmNixTargetGenerator
{
public:
  void Generate();
  void WriteObjectDerivations();
  void WriteLinkDerivation();
  
private:
  std::string GetDerivationName(cmSourceFile* sf);
  std::vector<std::string> GetSourceDependencies(cmSourceFile* sf);
};
```

### Dependency Analysis Integration

The backend will integrate with CMake's existing dependency scanning:
- `cmDepends` classes for header dependency analysis
- `cmGeneratorTarget` for source file enumeration
- `cmSourceFile` for per-file metadata

### Testing Strategy

#### Unit Tests
- Generator registration
- Derivation generation logic  
- Dependency mapping accuracy

#### Integration Tests
Create test projects in `Tests/RunCMake/Nix/`:
- Simple C executable
- Static library + executable
- Multi-source projects
- Header dependency changes

#### Validation Tests
- Build generated Nix expressions with `nix-build`
- Verify incremental build behavior
- Test parallel build execution

## Success Criteria

### Phase 1 Success Criteria
- [ ] CMake generates valid `default.nix` for simple C projects
- [ ] Generated Nix expressions build successfully with `nix-build`
- [ ] One derivation per translation unit is created
- [ ] Dependency changes trigger minimal rebuilds
- [ ] All existing CMake tests pass with Nix backend disabled

### Phase 2 Success Criteria  
- [ ] Header dependency tracking works - projects with multiple files and headers build correctly
- [ ] Include path support enables building projects with headers in separate directories  
- [ ] Multi-configuration support works (Debug/Release builds)
- [ ] Compiler auto-detection functions correctly
- [ ] External dependencies are handled properly through Nix packages
- [ ] Generated derivations include proper `-I` and `-D` flags
- [ ] Complex projects (e.g., CMake itself) can be built with the Nix generator

### Phase 3 Success Criteria
- [ ] Feature parity with Unix Makefiles generator
- [ ] Performance comparable to other backends
- [ ] Integration with Nix ecosystem tools

## Risk Assessment

### Technical Risks
- **Nix learning curve**: Mitigation through prototype development
- **CMake complexity**: Leverage existing generator patterns
- **Dependency accuracy**: Extensive testing with real projects

### Integration Risks
- **CMake upstream acceptance**: Follow CMake contribution guidelines
- **Build system compatibility**: Ensure no regression in existing functionality

## Timeline

### Phase 1: 8-12 weeks
- Weeks 1-2: Generator skeleton and registration
- Weeks 3-6: Core derivation generation
- Weeks 7-8: Basic dependency tracking
- Weeks 9-12: Testing and refinement

### Phase 2: 6-8 weeks
- Multi-configuration support
- Compiler detection
- Advanced dependency handling

### Phase 3: 8-10 weeks
- Custom commands
- Install rules
- Performance optimization

## Documentation Requirements

### User Documentation
- `CMake/Help/generator/Nix.rst` - User-facing generator documentation
- Usage examples and best practices
- Integration with Nix development workflows

### Developer Documentation
- Code comments following CMake conventions
- Architecture decisions and design rationale
- Testing procedures and validation methods

## Validation Against Existing Code

Based on analysis of `cmGlobalNinjaGenerator` and `cmGlobalUnixMakefileGenerator3`:

### Confirmed Patterns
- ✅ Generator registration in `cmake.cxx` AddDefaultGenerators()
- ✅ Factory pattern using `cmGlobalGeneratorSimpleFactory<T>`
- ✅ Inheritance from `cmGlobalCommonGenerator`
- ✅ Local generator creation via `CreateLocalGenerator()`
- ✅ Target enumeration through `cmGeneratorTarget`

### Documentation Validation
- ✅ Generator docs follow established format in `Help/generator/`
- ✅ Built-in targets (all, install, etc.) are standard across generators
- ✅ Per-subdirectory target generation is expected

### Dependency Tracking Validation
- ✅ CMake has extensive dependency analysis infrastructure
- ✅ `GetSourceFiles()` provides translation unit enumeration
- ✅ Header dependencies are tracked by existing `cmDepends` classes

This PRD provides a comprehensive roadmap for implementing a Nix backend that leverages CMake's existing architecture while providing the fine-grained build caching that Nix enables. 