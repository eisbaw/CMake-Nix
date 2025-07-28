# CMake Nix Backend - TODO Status


### Potential Future Enhancements (Low Priority):
While the Nix backend is production-ready, these features could be added if needed:

1. **CTest Integration**:
   - Support for `add_test()` and `enable_testing()`
   - Integration with CTest test runners
   - Test property support

2. **Advanced Custom Commands**:
   - `PRE_BUILD`, `POST_BUILD`, `PRE_LINK` timing support
   - `BYPRODUCTS` handling for better dependency tracking
   - Generator expressions in custom command arguments

3. **Additional Target Types**:
   - macOS framework targets
   - Windows DLL support (if Nix ever supports Windows)
   - `ALIAS` target enhancements

4. **Performance Optimizations**:
   - Parallel header scanning for very large projects
   - Incremental regeneration support
   - Memory usage optimization for 10,000+ file projects

### Production Status:
The CMake Nix backend is feature-complete and production-ready:
- Successfully builds CMake itself
- Handles complex real-world projects
- Performance optimized with advanced caching
- Comprehensive error handling and validation
- Well-documented with examples and best practices

## Code Quality Issues from Architecture Review (2025-07-28)

### Architecture Issues (MPED Architect Review):
1. **God Class Anti-Pattern**:
   - `cmGlobalNixGenerator` is over 4,500 lines doing too many things
   - Should be decomposed into focused modules (derivation writer, dependency resolver, etc.)
   - Mixed abstraction levels throughout
   
   **Refactoring Task Breakdown**:
   a) DONE Extract derivation writing functionality from cmGlobalNixGenerator into separate cmNixDerivationWriter class
      - DONE Create cmNixDerivationWriter class with clean interface
      - DONE Move WriteNixHelperFunctions, WriteFilesetUnion, WriteCompositeSource methods
      - DONE Refactor WriteObjectDerivation to use the new class (partial delegation)
      - TODO: Refactor WriteLinkDerivation to use the new class  
      - TODO: Refactor WriteCustomCommandDerivations to use the new class
   
   b) Extract dependency graph management from cmGlobalNixGenerator into cmNixDependencyGraph class
      - Move BuildDependencyGraph method and DependencyGraph nested class
      - Move ObjectDerivations map and related dependency tracking
      - Implement proper graph algorithms in isolated component
   
   c) Extract custom command handling from cmGlobalNixGenerator into cmNixCustomCommandHandler class
      - Move CustomCommandInfo struct and CustomCommands vector
      - Move WriteCustomCommandDerivations and CollectCustomCommands methods
      - Move custom command cycle detection logic (300+ lines)
   
   d) Extract install rule generation from cmGlobalNixGenerator into cmNixInstallRuleGenerator class
      - Move WriteInstallRules, WriteInstallOutputs, CollectInstallTargets methods
      - Move InstallTargets vector and related state
      - Create focused install handling component
   
   e) Extract header dependency processing from cmGlobalNixGenerator into cmNixHeaderDependencyResolver class
      - Move ProcessHeaderDependencies, FilterProjectHeaders methods
      - Move ExternalHeaderDerivations map and GetOrCreateHeaderDerivation
      - Consolidate header scanning logic
   
   f) Create cmNixCacheManager to consolidate all caching logic
      - Move DerivationNameCache, LibraryDependencyCache, TransitiveDependencyCache
      - Implement clear cache invalidation strategies
      - Add proper memory limits and eviction policies
   
   g) Move compiler detection and configuration logic to enhanced cmNixCompilerResolver
      - Move DetermineCompilerPackage, GetCompilerPackage, GetCompilerCommand methods
      - Consolidate compiler-specific logic in one place
   
   h) Create cmNixBuildConfiguration class to handle build config logic
      - Move GetBuildConfiguration method
      - Move configuration-specific flag generation
      - Separate build configuration concerns
   
   i) Extract file system operations into cmNixFileSystemHelper
      - Move IsSystemPath, path validation logic
      - Consolidate file I/O operations
      - Add proper error handling for file operations
   
   j) Create integration tests for the refactored components
      - Test each extracted component in isolation
      - Test component interactions
      - Ensure no regression in functionality

2. **State Management Problems**:
   - Multiple mutex-protected caches with unclear ownership
   - State duplicated between different tracking structures
   - Should consolidate into single source of truth

3. **MPED Principle Violations**:
   - Silent failures in error handling (e.g., `cmNixPackageMapper::LoadMappingsFromFile`)

### Test Coverage Gaps (Scout Testing Advocate Review):
1. **Missing Critical Tests**:
   - No integration tests for actual Nix file generation
   - No tests for error handling and recovery scenarios
   - Missing tests for concurrent/parallel build scenarios
   - No property-based testing for input validation

2. **Security-Critical Path Testing**:
   - `ValidatePathSecurity()` - untested
   - Symlink resolution and circular symlink handling - untested
   - Path traversal attack prevention - untested
   - Unicode normalization in paths - untested

3. **Thread Safety Testing**:
   - Concurrent access to singleton cmNixPackageMapper - untested
   - Race conditions in cache access - untested
   - Parallel header scanning - untested

### Maintainability Issues (Keeper Maintainer Review):
1. **Monolithic Implementation**:
   - Main file over 4,500 lines with deep nesting (5+ levels)
   - Complex dependency graph embedded in main class
   - 300+ lines just for custom command cycle detection

2. **Technical Debt**:
   - `CMAKE_NIX_IGNORE_CIRCULAR_DEPS` flag is dangerous workaround
   - Configuration-time file embedding could hit Nix expression limits
   - Hard-coded Unix conventions throughout

3. **Documentation Gaps**:
   - Missing architectural documentation for dependency graph algorithm
   - Caching strategy not documented
   - Thread safety guarantees unclear
   - Performance characteristics undocumented

### Domain Clarity Issues (Compass Domain Expert Review):
1. **Mixed Domain Concerns**:
   - CMake and Nix concepts not clearly separated
   - `cmGlobalNixGenerator` handles both CMake processing AND Nix generation
   - Path handling mixes CMake resolution with Nix store concepts

2. **Poor Business Language**:
   - Technical terms used without business context
   - Method names like `WriteObjectDerivation` don't express intent
   - Constants mix technical and domain concepts

3. **Scattered Business Rules**:
   - Compiler selection logic buried in string comparisons
   - Build configuration rules mixed with file I/O
   - Dependency resolution combines multiple concerns

### Tooling Enhancement Opportunities (Forge Tooling Expert Review):
1. **Development Environment**:
   - shell.nix missing modern tools (ccache, profilers, static analyzers)
   - No pre-commit hooks configured
   - Missing development helpers and aliases

2. **Build Automation**:
   - justfile could use format, lint-fix, watch commands
   - No automated benchmarking or profiling
   - Missing code coverage generation

3. **Developer Experience**:
   - No .envrc for direnv users
   - Missing IDE configuration files
   - Incomplete developer documentation

### Recommended Refactoring Priority:
1. **High Priority**:
   - Extract dependency graph into separate component
   - Add security-critical path validation tests
   - Document critical algorithms and caching strategy

2. **Medium Priority**:
   - Decompose cmGlobalNixGenerator into focused modules
   - Add thread safety and concurrency tests
   - Create explicit domain boundaries between CMake and Nix

3. **Low Priority**:
   - Enhance development tooling
   - Add property-based testing
   - Improve error messages with context
