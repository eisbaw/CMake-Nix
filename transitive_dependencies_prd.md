# Product Requirements Document: Transitive Dependency Resolution in CMake Nix Generator

## 1. Problem Statement

The CMake Nix generator currently fails to properly handle transitive shared library dependencies. When an executable depends on a shared library that itself depends on other shared libraries, the generator doesn't include the transitive dependencies in the final link command, causing "DSO missing from command line" errors.

**Current Issue:**
```nix
# Generated (INCORRECT)
link_complex_app = stdenv.mkDerivation {
  buildInputs = [ gcc link_engine_lib ];
  buildPhase = ''
    g++ $objects ${link_engine_lib}/libengine_lib.so -o "$out"
  '';
};
```

**Required Solution:**
```nix
# Needed (CORRECT)
link_complex_app = stdenv.mkDerivation {
  buildInputs = [ gcc link_engine_lib link_math_lib ];
  buildPhase = ''
    g++ $objects ${link_engine_lib}/libengine_lib.so ${link_math_lib}/libmath_lib.so -o "$out"
  '';
};
```

## 2. Goals & Objectives

### Primary Goals
- ✅ Resolve transitive shared library dependencies in generated Nix expressions
- ✅ Ensure proper build order through Nix's dependency system
- ✅ Fix the `test_complex_dependencies` failure
- ✅ Maintain compatibility with existing simple dependency cases

### Secondary Goals
- 🎯 Improve build performance by leveraging Nix's parallel building
- 🎯 Provide clear error messages when dependency cycles are detected
- 🎯 Support complex real-world dependency graphs (like OpenCV)

## 3. Technical Approach

### 3.1 Dependency Graph Construction
Build a complete dependency DAG during the CMake generation phase:
- **Nodes**: CMake targets (executables, shared libraries, static libraries)
- **Edges**: `target_link_libraries()` relationships
- **Metadata**: Target type, output paths, required transitive dependencies

### 3.2 Transitive Dependency Resolution
For each target, compute the complete set of shared library dependencies:
- Traverse the DAG to find all reachable shared library targets
- Filter out static libraries (they're embedded, not linked)
- Resolve circular dependencies and report errors
- Cache results to avoid redundant computation

### 3.3 Nix Expression Generation
Generate Nix derivations with complete dependency information:
- Include all transitive shared libraries in `buildInputs`
- Add all required library paths to the link command
- Ensure proper derivation import relationships

## 4. Implementation Plan

### Phase 1: Dependency Graph Infrastructure (Week 1-2)
```cpp
// New classes to add to cmGlobalNixGenerator
class cmNixDependencyNode {
    std::string targetName;
    cmStateEnums::TargetType type;
    std::vector<std::string> directDependencies;
    std::set<std::string> transitiveDependencies; // cached
};

class cmNixDependencyGraph {
    std::map<std::string, cmNixDependencyNode> nodes;
    
public:
    void AddTarget(const std::string& name, cmGeneratorTarget* target);
    void AddDependency(const std::string& from, const std::string& to);
    std::set<std::string> GetTransitiveSharedLibraries(const std::string& target);
    bool HasCircularDependency();
    std::vector<std::string> GetBuildOrder();
};
```

**Steps:**
1. **Create dependency graph data structures**
   - `cmNixDependencyNode` class for target representation
   - `cmNixDependencyGraph` class for graph operations
   - Integration points in `cmGlobalNixGenerator`

2. **Implement graph construction**
   - Modify `Generate()` to build dependency graph
   - Parse `target_link_libraries()` relationships
   - Handle different target types (SHARED, STATIC, EXECUTABLE)

3. **Add transitive resolution algorithm**
   - Depth-first search for reachable shared libraries
   - Cycle detection and error reporting
   - Caching for performance optimization

### Phase 2: Nix Expression Enhancement (Week 3-4)

**Steps:**
4. **Modify WriteLinkDerivation() to include transitive dependencies**
   ```cpp
   // In cmGlobalNixGenerator::WriteLinkDerivation()
   auto transitiveDeps = dependencyGraph.GetTransitiveSharedLibraries(targetName);
   
   // Add to buildInputs
   for (const auto& dep : transitiveDeps) {
       nixFileStream << " link_" << dep;
   }
   
   // Add to link command
   for (const auto& dep : transitiveDeps) {
       linkCommand += " ${link_" + dep + "}/" + GetLibraryFileName(dep);
   }
   ```

5. **Update buildInputs generation**
   - Include all transitive shared library derivations
   - Maintain proper Nix import relationships
   - Handle complex dependency chains

6. **Enhance link command generation**
   - Add all required library paths to g++/clang command
   - Respect library ordering for proper linking
   - Handle platform-specific library naming (lib*.so, lib*.dylib)

### Phase 3: Testing & Validation (Week 5-6)

**Steps:**
7. **Fix test_complex_dependencies**
   - Verify the specific "DSO missing" error is resolved
   - Test both `complex_app` and `direct_app` scenarios
   - Ensure build performance is acceptable

8. **Regression testing**
   - Run full test suite (`just dev`)
   - Verify simple cases still work (test_multifile, test_headers)
   - Test edge cases (circular dependencies, missing targets)

9. **Performance optimization**
   - Profile dependency graph construction time
   - Optimize for large projects (hundreds of targets)
   - Cache transitive dependency calculations

### Phase 4: Advanced Features (Week 7-8)

**Steps:**
10. **Error handling improvements**
    - Clear messages for circular dependency detection
    - Warnings for potentially missing dependencies
    - Validation of library file existence

11. **Documentation and examples**
    - Update CMake Nix generator documentation
    - Add complex dependency examples
    - Performance tuning guidelines

## 5. Implementation Details

### 5.1 Key Files to Modify

**Primary Files:**
- `cmGlobalNixGenerator.cxx` - Core dependency graph logic
- `cmGlobalNixGenerator.h` - New dependency graph classes
- `cmLocalNixGenerator.cxx` - Target dependency collection

**Secondary Files:**
- `cmNixTargetGenerator.cxx` - Enhanced library dependency handling
- Test files in `test_complex_dependencies/`

### 5.2 Algorithm Pseudocode

```cpp
// Dependency resolution algorithm
std::set<std::string> GetTransitiveSharedLibraries(const std::string& target) {
    std::set<std::string> visited;
    std::set<std::string> result;
    std::stack<std::string> stack;
    
    stack.push(target);
    
    while (!stack.empty()) {
        std::string current = stack.top();
        stack.pop();
        
        if (visited.count(current)) continue;
        visited.insert(current);
        
        auto& node = nodes[current];
        
        // If this is a shared library (and not the starting target), include it
        if (current != target && node.type == cmStateEnums::SHARED_LIBRARY) {
            result.insert(current);
        }
        
        // Add direct dependencies to stack
        for (const auto& dep : node.directDependencies) {
            if (!visited.count(dep)) {
                stack.push(dep);
            }
        }
    }
    
    return result;
}
```

### 5.3 Generated Nix Expression Example

**Before (Broken):**
```nix
link_complex_app = stdenv.mkDerivation {
    name = "complex_app";
    buildInputs = [ gcc link_engine_lib ];
    objects = [ complex_app_main_cpp_o ];
    buildPhase = ''
      g++ $objects ${link_engine_lib}/libengine_lib.so -o "$out"
    '';
};
```

**After (Fixed):**
```nix
link_complex_app = stdenv.mkDerivation {
    name = "complex_app";
    buildInputs = [ gcc link_engine_lib link_math_lib ];  # Added transitive dep
    objects = [ complex_app_main_cpp_o ];
    buildPhase = ''
      g++ $objects \
        ${link_engine_lib}/libengine_lib.so \
        ${link_math_lib}/libmath_lib.so \    # Added transitive library
        -o "$out"
    '';
};
```

## 6. Success Criteria

### Must Have
- [ ] `test_complex_dependencies` passes without errors
- [ ] All existing tests continue to pass
- [ ] No performance regression > 20% on large projects
- [ ] Clear error messages for circular dependencies

### Should Have  
- [ ] OpenCV test works (if dependency-related failure)
- [ ] Support for 100+ target projects
- [ ] Build time improvement from Nix parallelization
- [ ] Comprehensive error handling

### Nice to Have
- [ ] Visual dependency graph output for debugging
- [ ] Dependency analysis tools
- [ ] Integration with CMake's dependency graphing

## 7. Risk Assessment

### High Risk
- **Circular dependency handling**: Complex projects might have cycles
- **Performance impact**: Large dependency graphs could slow generation
- **Compatibility**: Changes might break existing simple cases

### Medium Risk
- **Memory usage**: Large graphs could consume significant memory
- **Platform differences**: Library naming varies across systems
- **Error messages**: Complex dependency errors might be confusing

### Mitigation Strategies
- Implement robust cycle detection early
- Add comprehensive unit tests for graph algorithms
- Profile memory usage on large real-world projects
- Provide detailed logging for debugging dependency issues

## 8. Timeline

**Total Estimated Time: 6-8 weeks**

| Phase | Duration | Key Deliverables |
|-------|----------|------------------|
| Phase 1 | 2 weeks | Dependency graph infrastructure |
| Phase 2 | 2 weeks | Enhanced Nix expression generation |
| Phase 3 | 2 weeks | Testing and validation |
| Phase 4 | 2 weeks | Advanced features and optimization |

## 9. Plan Review

### ✅ **Strengths of This Plan**

1. **Addresses Root Cause**: Fixes the fundamental transitive dependency issue
2. **Leverages Nix Strengths**: Uses Nix's dependency system properly
3. **Incremental Implementation**: Can be built and tested phase by phase
4. **Performance Conscious**: Considers build time and memory impact
5. **Comprehensive Testing**: Includes regression testing and edge cases

### ⚠️ **Potential Concerns**

1. **Complexity**: Adds significant complexity to the generator
2. **Edge Cases**: Many corner cases in real-world dependency graphs
3. **Performance**: Could impact generation time for large projects
4. **Maintenance**: More complex code means more potential bugs

### 🔧 **Recommended Adjustments**

1. **Start with Simple Algorithm**: Begin with basic DFS, optimize later
2. **Add Extensive Logging**: Critical for debugging complex dependency issues  
3. **Incremental Rollout**: Test on simple cases before complex ones
4. **Performance Benchmarks**: Establish baselines early

### 📋 **Next Steps**

1. **Validate Approach**: Review with stakeholders
2. **Set up Development Environment**: Prepare test cases and benchmarks
3. **Begin Phase 1**: Start with dependency graph data structures
4. **Create Test Suite**: Comprehensive tests for graph algorithms

This plan provides a solid foundation for resolving the transitive dependency limitation while maintaining system reliability and performance.