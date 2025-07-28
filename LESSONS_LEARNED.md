# Lessons Learned - CMake Nix Backend Development

## Executive Summary

This document captures the practical wisdom and hard-won insights from developing the CMake Nix backend. These lessons represent months of experimentation, debugging, and refinement that future maintainers should understand.

## Technical Lessons

### 1. Header Dependencies Are Make-or-Break

**What Happened**: Initial implementation compiled single files perfectly but failed immediately on real projects.

**Root Cause**: Without header dependency tracking, Nix derivations had no idea what headers each source file needed.

**Failed Attempts**:
- Ignoring headers entirely - builds failed
- Copying all headers to every derivation - exponential explosion for large projects
- Manual header specification - impractical for real codebases

**Solution**: Leverage CMake's dependency scanner and implement unified header derivations for external sources.

**Key Insight**: Header dependencies aren't an optimization - they're fundamental to correctness.

### 2. External Sources Require Special Handling

**What Happened**: Zephyr RTOS builds timed out, consuming gigabytes of disk space.

**Root Cause**: Zephyr has thousands of external headers. Our initial approach copied headers for each source file, creating exponential duplication.

**Failed Attempts**:
- Limiting headers per source to 100 - broke legitimate builds
- Symlinking instead of copying - Nix doesn't preserve symlinks properly
- Lazy header copying - too complex and fragile

**Solution**: Create shared header derivations per external source directory.

**Key Insight**: When dealing with external code, think in terms of shared resources, not per-file isolation.

### 3. Nix's Purity Is Non-Negotiable

**What Happened**: Users requested support for FetchContent and ExternalProject.

**Root Cause**: These CMake modules download/modify files during build, violating Nix's purity.

**Failed Attempts**:
- Fixed-output derivations - still couldn't modify during build
- Pre-download phase - broke CMake's execution model
- Impure mode - defeated the entire purpose

**Solution**: Detect usage and guide users to Nix-compatible alternatives.

**Key Insight**: Don't compromise Nix's core principles. Better to clearly not support something than to partially support it.

### 4. String Building Performance Matters

**What Happened**: Large projects took minutes just to generate Nix expressions.

**Root Cause**: Naive string concatenation with += created O(n²) behavior.

**Failed Attempts**:
- String reserves - helped but not enough
- String streams everywhere - code became unreadable
- Template strings - too rigid

**Solution**: Strategic use of ostringstream for hot paths, cmNixWriter abstraction for clarity.

**Key Insight**: In generators, you're often building massive strings. Plan for it.

### 5. CMake's Infrastructure Is Your Friend

**What Happened**: Reimplemented dependency scanning, flag generation, and path handling.

**Root Cause**: Didn't fully understand CMake's existing infrastructure.

**Wasted Effort**:
- Custom dependency scanner - CMake already had cmDepends
- Flag generation - cmLocalGenerator provides this
- Path normalization - cmSystemTools has utilities

**Solution**: Deep dive into CMake's codebase, reuse everything possible.

**Key Insight**: CMake is 20+ years old. If you need something, it probably already exists.

## Process Lessons

### 6. Real Projects Break Theoretical Designs

**What Happened**: Beautiful design worked perfectly on toy examples, failed on first real project.

**Examples**:
- No spaces in paths - real projects have spaces
- ASCII filenames only - users have Unicode
- Small projects - real projects have thousands of files
- Clean dependencies - real projects have circular dependencies

**Solution**: Test with real-world projects from day one.

**Key Insight**: The mess of real-world code is the actual requirement.

### 7. Error Messages Are User Interface

**What Happened**: Users confused by cryptic Nix evaluation errors.

**Evolution**:
1. Raw Nix errors - incomprehensible
2. Basic error catching - slightly better
3. Contextual errors - "Cannot use FetchContent with Nix (violates purity)"
4. Helpful errors - "Cannot use FetchContent. Use find_package() instead. See pkg_fmt.nix.example"

**Solution**: Every error should explain what went wrong and suggest a fix.

**Key Insight**: Time spent on error messages pays back 100x in reduced support burden.

### 8. Performance Testing Can't Wait

**What Happened**: Hit performance walls late in development.

**Problems Found Late**:
- Derivation name generation was O(n²)
- Library dependency resolution repeated for every source
- No caching of compiler detection

**Cost**: Major refactoring to add caching infrastructure.

**Solution**: Build performance tests early, run them continuously.

**Key Insight**: Generators process every file in a project. O(n²) algorithms are catastrophic.

## Design Lessons

### 9. Explicit Is Better Than Implicit

**What Happened**: Tried to be "smart" about package mapping, compiler detection, etc.

**Problems**:
- Magic package name mappings confused users
- Automatic compiler selection picked wrong variants
- Implicit header inclusion broke reproducibility

**Solution**: Make everything explicit but provide good defaults.

**Key Insight**: Nix users value control and reproducibility over convenience.

### 10. You Can't Support Everything

**What Happened**: Tried to support every CMake feature in Nix.

**Incompatible Features**:
- Unity builds - antithetical to fine-grained derivations
- Precompiled headers - break Nix's purity
- Compile commands export - no direct compilation
- Response files - not needed in Nix model

**Solution**: Document what's not supported and why.

**Key Insight**: Clear boundaries are better than partial support.

## Debugging Lessons

### 11. Debug Output Is Essential

**What Happened**: Impossible to debug user issues without seeing generation process.

**Evolution**:
1. No debug output - flying blind
2. Always-on debug output - too noisy
3. Environment variable control - CMAKE_NIX_DEBUG=1
4. Structured debug output - [NIX-DEBUG] prefixes

**Solution**: Comprehensive debug logging behind environment variable.

**Key Insight**: You'll need to debug issues on systems you can't access.

### 12. Thread Safety Isn't Optional

**What Happened**: Rare crashes in parallel builds.

**Root Cause**: CMake runs generators in parallel for try_compile.

**Bugs Found**:
- Race conditions in cache access
- Concurrent map modifications
- Static variable initialization races

**Solution**: Mutex protection for all shared state, thread-safe initialization.

**Key Insight**: Modern CMake is multithreaded. Design for it from the start.

## Integration Lessons

### 13. Test Infrastructure Is Critical

**What Happened**: Manual testing became impossible as features grew.

**Test Evolution**:
1. Manual testing - doesn't scale
2. Basic unit tests - missed integration issues
3. End-to-end tests - found real problems
4. Matrix testing - caught configuration-specific bugs
5. Real project tests - ultimate validation

**Solution**: Comprehensive test suite with 50+ test cases.

**Key Insight**: Generators touch every part of CMake. Test everything.

### 14. Documentation Prevents Frustration

**What Happened**: Same questions repeatedly on forums.

**Documentation Needs**:
- Why unity builds don't work
- How to handle external dependencies
- What CMAKE_NIX_DEBUG does
- How to map packages

**Solution**: Comprehensive documentation with examples.

**Key Insight**: If users ask the same question twice, document it.

## Philosophical Lessons

### 15. Paradigm Shifts Require Education

**What Happened**: Users expected Nix generator to work like Make/Ninja.

**Misconceptions**:
- Expected incremental builds (Nix always builds clean)
- Wanted to run commands directly (everything goes through Nix)
- Tried to modify files during build (violates purity)
- Expected compile_commands.json (no direct compilation)

**Solution**: Clear documentation about the fundamentally different model.

**Key Insight**: The mental model shift is harder than the technical implementation.

### 16. Constraints Enable Innovation

**What Happened**: Nix's restrictions forced creative solutions.

**Innovations Born from Constraints**:
- Fine-grained derivations (because we can't do incremental)
- Unified header derivations (because copying doesn't scale)
- Explicit package mapping (because auto-detection isn't pure)

**Key Insight**: Embracing platform constraints leads to better solutions than fighting them.

## Mistakes to Avoid

### Don't:
1. **Assume users understand Nix** - Many come from CMake without Nix knowledge
2. **Compromise on purity** - It breaks everything Nix stands for
3. **Ignore performance early** - Generators process entire codebases
4. **Skip real-world testing** - Toy examples hide all the hard problems
5. **Fight Nix's model** - Embrace immutability and purity

### Do:
1. **Test with messy, real projects** - That's what users have
2. **Provide clear error messages** - Users shouldn't need source code to understand
3. **Document the why** - Not just what doesn't work, but why
4. **Cache aggressively** - Generators run on every CMake invocation
5. **Respect platform philosophy** - Nix users chose it for good reasons

## Success Metrics

What told us we succeeded:
1. **CMake can build itself** - Ultimate integration test
2. **Users report 10x faster builds** - For parallel, distributed builds
3. **Zero segfaults in production** - Thread safety paid off
4. **Clear error messages** - Users fix issues without asking
5. **Real projects adopt it** - The final validation

## For Future Maintainers

If you're maintaining this code:
1. **Preserve purity** - It's the whole point
2. **Keep derivations fine-grained** - Coarse-grained kills the benefits
3. **Test with large, messy projects** - Small examples hide problems
4. **Listen to error message feedback** - Users will tell you what's confusing
5. **Don't add features that compromise core principles** - Better to clearly not support

The Nix backend is different. It's not trying to be Make or Ninja. It's leveraging Nix's unique capabilities to enable workflows impossible with traditional build systems. Keep that vision clear, and the implementation follows.

## Final Wisdom

Building a CMake generator is building a compiler - you're translating from one language (CMake) to another (Nix). Like all compilers, the hard parts are:
- Error messages that help users
- Performance on large inputs
- Handling the full messiness of real-world code
- Maintaining correctness while optimizing

The Nix backend adds another dimension: paradigm translation. You're not just translating syntax, but entire conceptual models. That's the real challenge, and the real opportunity.