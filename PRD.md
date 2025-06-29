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
**Scope**: Multi-configuration, advanced dependency handling

#### 2.1 Multi-Configuration Support
- Generate per-configuration derivation sets
- Support Debug/Release/etc. configurations
- Implement `IsMultiConfig()` to return `true`

#### 2.2 Compiler Auto-Detection
- Detect available compilers (gcc, clang, etc.)
- Generate appropriate Nix buildInputs
- Handle cross-compilation scenarios

#### 2.3 Advanced Dependencies
- External library dependencies
- pkg-config integration
- CMake find_package() mapping to Nix packages

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
- [ ] Multi-configuration support works
- [ ] Compiler auto-detection functions correctly
- [ ] External dependencies are handled properly

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