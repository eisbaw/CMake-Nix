# Issue #004: Subdirectory Support

**Priority:** MEDIUM  
**Status:** Not Started  
**Category:** Project Structure  
**Estimated Effort:** 2-3 days

## Problem Description

Many CMake projects use subdirectories with their own CMakeLists.txt files. The Nix generator currently doesn't handle `add_subdirectory()` properly, limiting support to single-directory projects.

## Current Behavior

Projects with structure like:
```
project/
  CMakeLists.txt
  src/
    CMakeLists.txt
  lib/
    CMakeLists.txt
```

Don't generate proper Nix expressions for targets in subdirectories.

## Expected Behavior

The generator should:
1. Process all subdirectories added via add_subdirectory()
2. Generate derivations for targets in subdirectories
3. Handle relative paths correctly
4. Maintain proper target dependencies across directories

## Implementation Plan

### 1. Process Subdirectories
```cpp
void cmGlobalNixGenerator::Generate() {
  // First process all directories to collect targets
  for (auto& lg : this->LocalGenerators) {
    lg->Generate();
  }
  
  // Then generate unified default.nix
  this->WriteNixFile();
}
```

### 2. Handle Relative Paths
```cpp
std::string cmNixTargetGenerator::GetSourcePath(cmSourceFile const* source) {
  // Convert to path relative to top-level source directory
  std::string topSource = this->GlobalGenerator->GetCMakeInstance()->GetHomeDirectory();
  std::string fullPath = source->GetFullPath();
  
  return cmSystemTools::RelativePath(topSource, fullPath);
}
```

### 3. Collect Targets from All Directories
```cpp
void cmGlobalNixGenerator::WriteNixFile() {
  // Collect all targets from all local generators
  std::map<std::string, cmGeneratorTarget*> allTargets;
  
  for (auto& lg : this->LocalGenerators) {
    for (auto& target : lg->GetGeneratorTargets()) {
      if (this->ShouldGenerateTarget(target.get())) {
        allTargets[target->GetName()] = target.get();
      }
    }
  }
  
  // Generate derivations for all targets
  for (auto& [name, target] : allTargets) {
    cmNixTargetGenerator gen(target, this);
    gen.Generate();
  }
}
```

### 4. Update Source File Paths
```cpp
void cmNixTargetGenerator::WriteObjectDerivation() {
  // Make paths relative to project root
  std::string srcPath = this->GetSourcePath(source);
  
  content << "    unpackPhase = ''\n";
  content << "      cp -r $src " << tempDir << "\n";
  content << "    '';\n";
  
  content << "    buildPhase = ''\n";
  content << "      cd " << tempDir << "\n";
  content << "      gcc -c " << srcPath << " -o $out\n";
  content << "    '';\n";
}
```

## Test Cases

1. **Basic Subdirectory**
   ```cmake
   # Top-level CMakeLists.txt
   add_subdirectory(src)
   
   # src/CMakeLists.txt
   add_executable(myapp main.c)
   ```

2. **Multiple Subdirectories**
   ```cmake
   add_subdirectory(lib)
   add_subdirectory(app)
   # app depends on lib
   ```

3. **Nested Subdirectories**
   ```cmake
   add_subdirectory(src)
   # src/CMakeLists.txt
   add_subdirectory(core)
   add_subdirectory(ui)
   ```

4. **Cross-Directory Dependencies**
   ```cmake
   # lib/CMakeLists.txt
   add_library(mylib lib.c)
   
   # app/CMakeLists.txt
   add_executable(myapp main.c)
   target_link_libraries(myapp mylib)
   ```

## Acceptance Criteria

- [ ] add_subdirectory() is processed correctly
- [ ] Targets in subdirectories are included in default.nix
- [ ] Source paths are relative to project root
- [ ] Include paths work across directories
- [ ] Target dependencies work across directories
- [ ] Nested subdirectories work

## Related Files

- `Source/cmGlobalNixGenerator.cxx` - Process all directories
- `Source/cmLocalNixGenerator.cxx` - Handle per-directory generation
- `Source/cmNixTargetGenerator.cxx` - Fix path handling