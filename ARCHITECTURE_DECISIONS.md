# Architecture Decision Records - CMake Nix Backend

## Overview

This document captures the key architectural decisions and design rationale for the CMake Nix generator backend, preserving the institutional knowledge and context behind technical choices made during development.

## ADR-001: Fine-Grained Per-Translation-Unit Derivations

**Date**: 2024-Q3 (Initial Design)

### Context
The team faced a fundamental choice about granularity when designing the Nix backend. Traditional build systems compile multiple files together, while Nix allows arbitrarily fine-grained build units.

### Decision
Generate one Nix derivation per translation unit (source file) rather than per-target or per-directory.

### Rationale
- **Maximum Parallelism**: Each source file can compile independently on different machines
- **Perfect Caching**: When one file changes, only that file rebuilds
- **Content-Addressed Storage**: Nix automatically detects unchanged files
- **Distributed Builds**: Fine granularity enables better work distribution

### Consequences
- **Positive**: Massive parallelism potential, minimal rebuilds, perfect cache efficiency
- **Negative**: More derivations to manage, potential overhead for tiny projects
- **Trade-off Accepted**: The benefits for medium-to-large projects far outweigh the overhead

### Alternatives Considered
1. **Per-Target Derivations**: Would have been simpler but lost caching benefits
2. **Unity Build Style**: Completely antithetical to Nix's philosophy
3. **Hybrid Approach**: Too complex for marginal benefits

### Future Considerations
Could add heuristics to batch very small files if overhead becomes problematic.

## ADR-002: Inherit from cmGlobalCommonGenerator

**Date**: 2024-Q3

### Context
CMake has multiple generator base classes. The team needed to choose the appropriate inheritance hierarchy.

### Decision
Inherit from `cmGlobalCommonGenerator` rather than `cmGlobalGenerator` directly.

### Rationale
- **Code Reuse**: CommonGenerator provides useful utilities for non-Makefile generators
- **Future-Proofing**: Easier to adapt as CMake's generator infrastructure evolves
- **Consistency**: Aligns with modern generators like Ninja

### Historical Background
Early prototypes inherited directly from `cmGlobalGenerator`, but this required reimplementing common functionality.

### Consequences
- **Positive**: Less code duplication, better integration with CMake infrastructure
- **Negative**: Must understand CommonGenerator's assumptions
- **Trade-off Accepted**: The benefits of reuse outweigh the learning curve

## ADR-003: No Direct Compiler Execution

**Date**: 2024-Q4

### Context
Traditional generators execute compilers directly or generate scripts that do. The Nix model is fundamentally different.

### Decision
Never execute compilers directly. Instead, generate Nix expressions that describe the build.

### Rationale
- **Hermetic Builds**: Nix controls the entire build environment
- **Reproducibility**: Builds happen in isolated derivations
- **Purity**: No side effects during generation phase

### Consequences
- **Positive**: True reproducibility, better error isolation
- **Negative**: Cannot generate compile_commands.json
- **Trade-off Accepted**: Reproducibility is more valuable than IDE integration

### Alternatives Considered
1. **Hybrid Mode**: Generate both Nix expressions and compile commands - too complex
2. **Post-Build Extraction**: Extract commands from Nix logs - unreliable

### Lessons Learned
Users initially expected compile_commands.json support. Clear documentation about this limitation is essential.

## ADR-004: Header Dependency Tracking Architecture

**Date**: 2024-Q4

### Context
The team discovered that complex projects (like those with multiple source files including headers) wouldn't build without proper header dependency tracking.

### Decision
Implement comprehensive header dependency scanning using CMake's existing infrastructure, with special handling for external headers.

### Rationale
- **Correctness**: Without header tracking, rebuilds miss changes
- **CMake Integration**: Reuse existing dependency scanning
- **Performance**: Cache header dependencies to avoid repeated scanning

### Implementation Evolution
1. **Initial**: No header tracking - failed on multi-file projects
2. **Second Attempt**: Copy all headers - exploded for large projects
3. **Final**: Unified header derivations for external sources

### Consequences
- **Positive**: Correct incremental builds, handles complex projects
- **Negative**: More complex derivation generation
- **Trade-off Accepted**: Correctness is non-negotiable

### Key Innovation
The unified header derivation approach for external sources prevents the exponential copying problem while maintaining build isolation.

## ADR-005: Multi-Configuration as Separate Derivations

**Date**: 2024-Q4

### Context
CMake supports multiple build configurations (Debug, Release, etc). The team needed to map this to Nix's model.

### Decision
Generate separate derivation sets for each configuration rather than parameterizing derivations.

### Rationale
- **Simplicity**: Each derivation is self-contained
- **Caching**: Different configurations cached separately
- **Clarity**: Easy to understand what each derivation does

### Consequences
- **Positive**: Clean separation, better caching
- **Negative**: More derivations in the Nix file
- **Trade-off Accepted**: Clarity and caching win

### Historical Note
An early prototype tried to parameterize derivations with configuration, but this complicated the Nix expressions significantly.

## ADR-006: External Dependency Mapping via pkg_*.nix Files

**Date**: 2025-Q1

### Context
CMake's find_package() locates system libraries dynamically. Nix requires explicit dependency declaration.

### Decision
Map CMake package names to Nix packages through pkg_*.nix files that users can customize.

### Rationale
- **Flexibility**: Users control exact Nix packages used
- **Discoverability**: Clear what packages are needed
- **Compatibility**: Works with existing CMake projects

### Implementation Details
```nix
# pkg_ZLIB.nix
{ zlib }:
{
  buildInputs = [ zlib ];
}
```

### Consequences
- **Positive**: Clean integration with Nix ecosystem
- **Negative**: Users must create pkg files for new dependencies
- **Trade-off Accepted**: Explicit is better than implicit in Nix

### Evolution
Started with hardcoded mappings, evolved to configurable system after user feedback.

## ADR-007: Compiler Auto-Detection with Override Support

**Date**: 2025-Q1

### Context
Different projects use different compilers. The Nix backend needs to map these to Nix packages.

### Decision
Implement automatic compiler detection with user override variables (CMAKE_NIX_<LANG>_COMPILER_PACKAGE).

### Rationale
- **Convenience**: Works out-of-box for common cases
- **Flexibility**: Users can override for special compilers
- **Compatibility**: Respects CMake's compiler detection

### Implementation
Created `cmNixCompilerResolver` class to centralize logic and avoid duplication.

### Consequences
- **Positive**: Handles GCC, Clang, and other compilers automatically
- **Negative**: Must maintain compiler ID mappings
- **Trade-off Accepted**: Convenience worth the maintenance

## ADR-008: No Support for Mutable Build Operations

**Date**: 2025-Q1

### Context
Some build systems (like Zephyr RTOS) require mutable file operations during build. Nix derivations are pure and immutable.

### Decision
Do not support ExternalProject_Add, FetchContent, or other mutable operations. Provide clear error messages.

### Rationale
- **Nix Philosophy**: Purity is fundamental to Nix
- **Reproducibility**: Mutable operations break reproducibility
- **Clear Boundaries**: Better to fail clearly than partially work

### Consequences
- **Positive**: Maintains Nix's guarantees
- **Negative**: Some projects cannot use Nix backend
- **Trade-off Accepted**: Purity is non-negotiable

### User Guidance
When detected, generate skeleton pkg_*.nix files to help users migrate to find_package().

## ADR-009: Custom Command Derivations

**Date**: 2025-Q1

### Context
CMake's add_custom_command() generates files during build. This must map to Nix's pure model.

### Decision
Generate separate derivations for custom commands with explicit input/output tracking.

### Rationale
- **Isolation**: Each command runs in its own derivation
- **Dependency Tracking**: Outputs become inputs to other derivations
- **Reproducibility**: Commands run in controlled environment

### Challenges Overcome
- Working directory handling required special care
- Multiple output files needed careful dependency management
- Command resolution adapted for Nix environment

### Consequences
- **Positive**: Custom commands work reliably
- **Negative**: More complex than traditional generators
- **Trade-off Accepted**: Necessary for real-world projects

## ADR-010: Install Rules as Separate Derivations

**Date**: 2025-Q1

### Context
CMake install() commands need to work in Nix's immutable store.

### Decision
Generate separate install derivations that copy from build outputs.

### Rationale
- **Separation**: Build and install are distinct phases
- **Flexibility**: Can install subset of built targets
- **Nix Convention**: Follows standard Nix patterns

### Implementation Note
Each target with install rules gets its own install derivation, aggregated by a top-level install.

### Consequences
- **Positive**: Clean install process
- **Negative**: Additional derivations
- **Trade-off Accepted**: Follows Nix best practices

## Performance Optimization Decisions

## ADR-011: Aggressive Caching Strategy

**Date**: 2025-Q1

### Context
Generating thousands of derivations for large projects could be slow.

### Decision
Implement multi-level caching for derivation names, library dependencies, and compiler resolution.

### Rationale
- **Performance**: Avoid repeated computation
- **Scalability**: Handle projects with thousands of files
- **Memory Bounds**: Limit cache sizes to prevent bloat

### Implementation Details
- Thread-safe caches with mutex protection
- Lazy initialization with double-checked locking
- Size limits to prevent unbounded growth

### Consequences
- **Positive**: 10x speedup for large projects
- **Negative**: More complex code
- **Trade-off Accepted**: Performance critical for usability

## Lessons Learned

### What Worked Well
1. **Fine-grained derivations** - The core design decision proved correct
2. **Reusing CMake infrastructure** - Saved enormous implementation effort
3. **Clear error messages** - Users understand limitations immediately
4. **Comprehensive test suite** - Caught issues early and often

### What We'd Do Differently
1. **Earlier header dependency implementation** - Was critical for real projects
2. **Better external source handling** - Initial approach didn't scale
3. **More configuration options** - Users want to tune behavior
4. **Plugin architecture** - For project-specific adaptations

### Surprising Discoveries
1. **Zephyr's build model** is fundamentally incompatible with Nix
2. **Users love the parallelism** more than expected
3. **The debugging experience** is actually better with derivations
4. **External headers** were the hardest technical challenge

## Future Architecture Considerations

### Potential Enhancements
1. **Nix Flakes Support** - Modern Nix packaging format
2. **Binary Cache Integration** - Share compiled objects across teams
3. **Incremental Header Scanning** - Further performance optimization
4. **Cross-Compilation Support** - Full Nix cross-compilation integration

### Maintenance Guidelines
1. **Keep derivations pure** - Never compromise on reproducibility
2. **Fail clearly** - Better to error than silently misbehave
3. **Document limitations** - Users should understand boundaries
4. **Test extensively** - Every feature needs comprehensive tests

## Conclusion

The CMake Nix backend represents a fundamental rethinking of build systems, leveraging Nix's unique capabilities for unprecedented parallelism and caching. The architectural decisions prioritized correctness, reproducibility, and performance - in that order. While some traditional features (like unity builds) are incompatible with this model, the benefits for modern development workflows are substantial.

The key insight is that fine-grained derivations, while more complex to generate, unlock capabilities impossible with traditional build systems. This architecture serves as a foundation for a new generation of reproducible, distributed builds.