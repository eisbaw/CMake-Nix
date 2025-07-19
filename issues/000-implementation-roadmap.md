# CMake Nix Backend - Implementation Roadmap

**Created:** 2025-07-19  
**Status:** In Progress  
**Phase 2:** 75% Complete

## Executive Summary

The CMake Nix backend project has made significant progress with core functionality implemented. This roadmap outlines the remaining work needed to achieve a production-ready generator that can build real-world CMake projects.

## Current Implementation Status

### ✅ Completed Features
- Basic generator infrastructure and registration
- Per-translation-unit Nix derivations  
- Multi-file C/C++ compilation
- Include path support (`-I` flags)
- Build configurations (Debug/Release)
- Basic external library support with auto-generation
- Compiler-based header dependency scanning (Option B)
- Clean object naming with relative paths

### 🚧 In Progress
- Testing with real-world projects
- Performance optimization
- Documentation

### ❌ Not Started
- Compiler auto-detection
- Shared library support
- find_package() integration
- Subdirectory support
- Custom commands
- Install rules
- Multi-config generator mode

## Critical Path to Production

### Phase 1: Essential Fixes (1-2 weeks)

**Goal:** Fix fundamental issues blocking real projects

1. **Issue #001: Compiler Auto-Detection** [HIGH]
   - Currently hardcoded to gcc
   - Blocks: Clang users, cross-compilation
   - Effort: 1-2 days

2. **Issue #002: Shared Library Support** [HIGH]
   - Only static libraries work
   - Blocks: Most modern C++ projects
   - Effort: 2-3 days

3. **Issue #003: find_package() Integration** [HIGH]
   - No support for CMake's package discovery
   - Blocks: Projects with external dependencies
   - Effort: 3-5 days

### Phase 2: Core Functionality (2-3 weeks)

**Goal:** Support majority of CMake projects

4. **Issue #004: Subdirectory Support** [MEDIUM]
   - add_subdirectory() doesn't work properly
   - Blocks: Multi-directory projects
   - Effort: 2-3 days

5. **Issue #005: Performance Optimization** [MEDIUM]
   - May be slow for large projects
   - Needed for: Production adoption
   - Effort: 3-4 days

### Phase 3: Advanced Features (3-4 weeks)

**Goal:** Full feature parity with other generators

6. **Issue #006: Custom Commands** [MEDIUM]
   - No support for code generation
   - Blocks: Protobuf, Qt MOC, etc.
   - Effort: 3-5 days

7. **Issue #007: Install Rules** [MEDIUM]
   - Can't create installable packages
   - Blocks: Distribution use cases
   - Effort: 2-3 days

8. **Issue #008: Multi-Config Generator** [LOW]
   - Single configuration only
   - Nice to have: IDE integration
   - Effort: 4-5 days

### Phase 4: Validation & Polish (2 weeks)

**Goal:** Production readiness

9. **Issue #009: Comprehensive Test Suite** [HIGH]
   - Ensure reliability and catch regressions
   - Effort: 3-4 days

10. **Issue #010: CMake Self-Hosting** [HIGH]
    - Ultimate validation test
    - Effort: 1-2 weeks

11. **Issue #011: Documentation** [MEDIUM]
    - User guide and examples
    - Effort: 2-3 days

## Implementation Priority

### Week 1-2: Critical Fixes
- [ ] #001 - Compiler auto-detection
- [ ] #002 - Shared library support
- [ ] Start #003 - find_package() integration

### Week 3-4: Core Features
- [ ] Complete #003 - find_package() integration
- [ ] #004 - Subdirectory support
- [ ] Start #009 - Test suite development

### Week 5-6: Advanced Features
- [ ] #006 - Custom commands
- [ ] #007 - Install rules
- [ ] Continue test development

### Week 7-8: Validation
- [ ] #010 - CMake self-hosting attempt
- [ ] #005 - Performance optimization based on findings
- [ ] #011 - Complete documentation

### Week 9-10: Polish
- [ ] Fix issues found during validation
- [ ] #008 - Multi-config if time permits
- [ ] Final testing and documentation

## Success Metrics

### Minimum Viable Product (MVP)
- [x] Build simple C/C++ projects
- [ ] Support external libraries via find_package()
- [ ] Handle multi-directory projects
- [ ] Generate working shared libraries
- [ ] Pass basic test suite

### Production Ready
- [ ] Build CMake itself
- [ ] Performance within 2x of Makefiles
- [ ] Comprehensive test coverage
- [ ] Complete documentation
- [ ] Support for 80% of CMake features

### Stretch Goals
- [ ] Multi-config generator support
- [ ] Windows platform support
- [ ] Integration with Nix flakes
- [ ] Performance parity with Ninja

## Risk Mitigation

### Technical Risks
1. **Performance at scale** - Mitigate with batching and profiling
2. **Nix limitations** - Document workarounds, engage Nix community
3. **CMake API changes** - Follow CMake development, maintain compatibility

### Resource Risks
1. **Complexity underestimation** - Built buffer into estimates
2. **Unknown unknowns** - Regular validation against real projects
3. **Maintenance burden** - Comprehensive test suite, clear documentation

## Dependencies

### External Dependencies
- Nix package manager (2.x or later)
- C/C++ compiler (gcc or clang)
- CMake build system (for development)

### Internal Dependencies
- CMake's generator infrastructure
- CMake's dependency scanning system
- CMake's target system

## Next Steps

1. **Immediate Action**: Start with Issue #001 (Compiler Auto-Detection)
2. **Parallel Work**: Begin test suite development alongside feature work
3. **Regular Validation**: Test against real projects weekly
4. **Community Engagement**: Share progress, gather feedback

## Long-term Vision

Once the Nix generator reaches production readiness:

1. **Upstream to CMake** - Contribute to official CMake
2. **Nix Integration** - Deep integration with Nix ecosystem
3. **Performance Leader** - Fastest incremental builds
4. **Developer Experience** - Best-in-class caching and reproducibility

## Conclusion

The CMake Nix backend is well-positioned to provide unique value through fine-grained caching and reproducible builds. With focused effort over the next 8-10 weeks, we can achieve a production-ready generator that enables new workflows for C/C++ development.

## Issue Tracker

| # | Title | Priority | Status | Effort |
|---|-------|----------|--------|--------|
| 001 | Compiler Auto-Detection | HIGH | Not Started | 1-2 days |
| 002 | Shared Library Support | HIGH | Not Started | 2-3 days |
| 003 | find_package() Integration | HIGH | Not Started | 3-5 days |
| 004 | Subdirectory Support | MEDIUM | Not Started | 2-3 days |
| 005 | Performance Optimization | MEDIUM | Not Started | 3-4 days |
| 006 | Custom Commands | MEDIUM | Not Started | 3-5 days |
| 007 | Install Rules | MEDIUM | Not Started | 2-3 days |
| 008 | Multi-Config Generator | LOW | Not Started | 4-5 days |
| 009 | Comprehensive Test Suite | HIGH | Not Started | 3-4 days |
| 010 | CMake Self-Hosting | HIGH | Not Started | 1-2 weeks |
| 011 | Documentation | MEDIUM | Not Started | 2-3 days |