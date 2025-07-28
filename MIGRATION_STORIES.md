# Migration Stories - Real Projects Using CMake Nix Backend

## Introduction

This document captures real-world experiences of projects migrating to the CMake Nix backend. Each story includes the challenges faced, solutions discovered, and lessons learned. These narratives preserve crucial institutional knowledge about what works, what doesn't, and why.

## Story 1: CMake Self-Hosting - The Ultimate Test

### Project Context
- **Size**: ~800 source files, 100K+ lines of code
- **Complexity**: Bootstrapping, multiple languages, code generation
- **Timeline**: 2 weeks of intensive work

### Initial Attempt (Day 1-3)
```
Error: Bootstrap executable not found
Error: Cannot find cmConfigure.h
Error: Circular dependency in cmSystemTools
... 2000+ more errors
```

### Challenges Discovered
1. **Bootstrap Process**: CMake builds a minimal version of itself first
   - Solution: Special bootstrap derivation with minimal dependencies
   - Lesson: Self-hosting tools have unique requirements

2. **Generated Headers**: cmVersionConfig.h generated during configure
   - Initial approach: Include in every derivation (wasteful)
   - Final solution: Separate derivation for configured headers
   - Lesson: Code generation requires careful dependency modeling

3. **Circular Dependencies**: Many CMake classes interdependent
   - Solution: Careful link order in derivations
   - Lesson: Real codebases aren't DAGs

4. **Platform Checks**: CMake does extensive platform testing
   - Challenge: try_compile in pure environment
   - Solution: Parallel try_compile support
   - Lesson: Build systems are also discovery systems

### Success Factors
- **Day 14**: First successful build
- **Performance**: 8 minutes on 32 cores (vs 45 minutes single-threaded)
- **Validation**: Binary identical to traditional build

### Key Insight
"Self-hosting forced us to handle every edge case. If CMake can build itself with Nix, anything can."

## Story 2: The Zephyr RTOS Disaster

### Project Context
- **Size**: 5000+ source files, massive external header tree
- **Complexity**: Embedded RTOS with hardware abstraction layers
- **Timeline**: 1 week before declaring incompatibility

### The Problem Manifests (Day 1)
```bash
$ cmake -G Nix ..
[5 hours later, still running]
$ du -h build/
47G     build/
```

### Investigation Revealed
1. **External Headers**: 10,000+ headers in external trees
2. **Per-Source Copying**: Each source copied all possible headers
3. **Exponential Growth**: 5000 sources × 10000 headers = disaster

### Attempted Solutions
1. **Header Limit** (Day 2)
   ```cmake
   set(CMAKE_NIX_EXTERNAL_HEADER_LIMIT 100)
   ```
   - Result: Builds failed, missing required headers

2. **Selective Copying** (Day 3)
   - Tried to predict which headers needed
   - Too complex, still missed dependencies

3. **Unified Headers** (Day 4)
   - One derivation per external directory
   - Shared by all sources
   - Success! But...

### The Final Incompatibility (Day 5)
Zephyr's build process:
1. Modifies source files during build
2. Generates code based on configuration
3. Patches headers dynamically

All fundamental violations of Nix purity.

### Lessons Learned
- **Not every project can use Nix**: Some require mutable builds
- **External code is different**: Can't treat it like project code
- **Know when to stop**: Document incompatibility clearly

### Documentation Created
Created `zephyr_mutable_env.md` explaining why Zephyr can't work with Nix.

## Story 3: Scientific Computing Library Success

### Project Context
- **Size**: 200 source files, heavy template usage
- **Complexity**: Complex mathematical algorithms, OpenMP
- **External Deps**: BLAS, LAPACK, OpenMPI

### Migration Process (Week 1)

#### Day 1: Initial Setup
```cmake
cmake -G Nix -DCMAKE_BUILD_TYPE=Release ..
```
Immediate failure: "Cannot find BLAS"

#### Day 2: Package Mapping
Created `pkg_BLAS.nix`:
```nix
{ blas }:
{
  buildInputs = [ blas ];
}
```

Similar files for LAPACK, OpenMPI.

#### Day 3: OpenMP Discovery
Compiler flags included `-fopenmp` but link failed.

Solution in `pkg_OpenMP.nix`:
```nix
{ gcc }:
{
  buildInputs = [ gcc ];
  # OpenMP included in gcc
}
```

#### Day 4: First Successful Build
- 12 minutes on 8 cores
- Previous Make build: 45 minutes

#### Day 5: Distributed Build Setup
```nix
# /etc/nix/machines
builder@remote x86_64-linux 32 2 big-parallel
```

Result: 3 minutes across multiple machines!

### Unexpected Benefits
1. **Reproducible Research**: Papers can reference exact Nix derivations
2. **Cluster Deployment**: Same derivations work everywhere
3. **Dependency Documentation**: pkg_*.nix files document all requirements

### Quote from Lead Developer
"We spent years fighting dependency hell on different clusters. Nix solved it in a week."

## Story 4: The Game Engine Migration

### Project Context
- **Size**: 2000+ sources, 50+ third-party libraries
- **Complexity**: Graphics, physics, audio, networking
- **Challenge**: Vendor libraries with custom build steps

### The FetchContent Problem (Day 1)
```cmake
FetchContent_Declare(
  bullet
  GIT_REPOSITORY https://github.com/bulletphysics/bullet3
  GIT_TAG 3.24
)
```

Error: "FetchContent incompatible with Nix (downloads during build)"

### Migration Strategy

#### Step 1: Inventory Dependencies
Found 23 FetchContent calls, 15 ExternalProject_Add uses.

#### Step 2: Create Package Files
Generated skeleton files:
```bash
[NIX] Generated pkg_bullet.nix.example - please review and rename
[NIX] Generated pkg_sdl2.nix.example - please review and rename
...
```

#### Step 3: Convert to find_package
```cmake
# Before
FetchContent_MakeAvailable(bullet)

# After
find_package(Bullet REQUIRED)
```

#### Step 4: Vendor Stubborn Dependencies
Some libraries wouldn't work with find_package.
Solution: Git submodules for source-only dependencies.

### Performance Results
- **Traditional build**: 2 hours full rebuild
- **Nix backend**: 15 minutes full build (distributed)
- **Incremental**: 10 seconds (only changed files)

### Challenges Overcome
1. **Binary Assets**: Game assets aren't source code
   - Solution: Separate Nix derivation for assets
   
2. **Shader Compilation**: Custom build steps
   - Solution: Custom command derivations
   
3. **Platform-Specific Code**: Windows/Mac/Linux variants
   - Solution: Nix only supports Linux anyway

### Team Feedback
"The initial migration was painful, but now our CI builds are 10x faster and actually reproducible."

## Story 5: Enterprise Migration - 10 Million LOC

### Project Context
- **Size**: 10M+ lines across 500+ CMake projects
- **Complexity**: 20 years of accumulated build logic
- **Teams**: 200+ developers across 5 locations

### Phased Approach (6 Months)

#### Phase 1: Pilot Project (Month 1)
- Selected small, clean library
- Proved concept, trained first adopters
- Created migration playbook

#### Phase 2: Critical Libraries (Month 2-3)
- Core libraries used everywhere
- Discovered interconnected dependencies
- Built dependency graph visualization

#### Phase 3: The Monolith (Month 4-5)
- Main application: 2M lines
- 3000+ source files
- Custom code generators

Challenges:
1. **Generated Code**: 30+ code generators
   - Solution: Custom command derivation for each
   
2. **Circular Dependencies**: Legacy code had cycles
   - Solution: Refactored some, documented others
   
3. **Build Machine Resources**: Not enough RAM
   - Solution: Distributed builds across data center

#### Phase 4: Full Rollout (Month 6)
- All projects migrated
- CI/CD pipeline updated
- Developer training completed

### Metrics
- **Build Time**: 4 hours → 25 minutes (distributed)
- **Cache Hit Rate**: 85% across team
- **Developer Satisfaction**: Way up

### Cultural Changes
1. **No more "works on my machine"**: Builds identical everywhere
2. **Fearless refactoring**: Know exactly what depends on what
3. **Fast CI**: Pull requests build in minutes, not hours

### Challenges That Remain
1. **Windows Developers**: Need WSL or VMs
2. **Learning Curve**: New developers need Nix training
3. **Legacy Scripts**: Some build scripts too complex to migrate

### Executive Quote
"The migration cost was significant, but we're saving 2 hours per developer per day. ROI in 3 months."

## Common Patterns Across Migrations

### What Works Well
1. **Clean CMake projects** migrate easily
2. **find_package() users** have smooth transition
3. **Linux-only projects** perfect fit
4. **Distributed teams** benefit most

### What Causes Pain
1. **FetchContent/ExternalProject** requires rewriting
2. **Windows dependencies** need alternatives
3. **Mutable build steps** fundamental incompatibility
4. **Generated files** need careful modeling

### Success Factors
1. **Executive buy-in**: Migrations need support
2. **Pilot projects**: Prove value early
3. **Training**: Invest in developer education
4. **Gradual migration**: Don't do everything at once

### Failure Patterns
1. **Forcing incompatible projects**: Some can't use Nix
2. **Skipping education**: Developers revolt
3. **Underestimating effort**: Always takes longer
4. **Ignoring Windows users**: Need alternative strategy

## Migration Checklist

Based on these stories, before migrating:

- [ ] Inventory all FetchContent/ExternalProject usage
- [ ] Check for mutable build operations
- [ ] Assess Windows/Mac requirements
- [ ] Count external dependencies
- [ ] Measure current build times
- [ ] Identify code generation steps
- [ ] Check for circular dependencies
- [ ] Plan phased approach
- [ ] Allocate training time
- [ ] Set up distributed builders

## Conclusion

These migration stories show that success with the CMake Nix backend depends more on project characteristics and team commitment than technical complexity. Projects that embrace Nix's philosophy see dramatic improvements. Those that fight it struggle.

The key lesson across all stories: **Nix is different**. It's not a faster Make or a better Ninja. It's a fundamentally different approach to builds. Projects that understand and embrace this difference succeed. Those that don't should stick with traditional generators.

Remember: Every successful migration started with someone saying "our builds take too long" and ended with "why didn't we do this sooner?" The journey between those points is what these stories capture.