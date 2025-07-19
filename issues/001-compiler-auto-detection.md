# Issue #001: Compiler Auto-Detection

**Priority:** HIGH  
**Status:** Not Started  
**Category:** Core Functionality  
**Estimated Effort:** 1-2 days

## Problem Description

The Nix generator currently hardcodes `gcc` as the compiler in all generated derivations. This prevents:
- Using Clang or other compilers
- Cross-compilation scenarios
- Respecting user's CMAKE_C_COMPILER/CMAKE_CXX_COMPILER choices
- Proper compiler feature detection

## Current Behavior

In `cmNixTargetGenerator::WriteObjectDerivation()`:
```cpp
// Currently hardcoded:
content += "    buildInputs = [ gcc ];\n";
```

## Expected Behavior

The generator should:
1. Detect compiler from CMAKE_C_COMPILER and CMAKE_CXX_COMPILER variables
2. Map CMake compiler paths to appropriate Nix packages
3. Generate correct buildInputs for the detected compiler
4. Support common compilers: gcc, clang, cross-compilers

## Implementation Plan

### 1. Detect Compiler Type
```cpp
std::string cmNixTargetGenerator::GetCompilerPackage(const std::string& lang) {
  cmGeneratorTarget* target = this->GeneratorTarget;
  cmMakefile* mf = target->GetMakefile();
  
  std::string compiler = mf->GetSafeDefinition("CMAKE_" + lang + "_COMPILER");
  std::string compilerId = mf->GetSafeDefinition("CMAKE_" + lang + "_COMPILER_ID");
  
  // Map compiler ID to Nix package
  if (compilerId == "GNU") {
    return "gcc";
  } else if (compilerId == "Clang") {
    return "clang";
  } else if (compilerId == "AppleClang") {
    return "clang";
  }
  
  // Default fallback
  return "gcc";
}
```

### 2. Update Object Derivation Generation
```cpp
void cmNixTargetGenerator::WriteObjectDerivation(
  std::ostream& content,
  cmSourceFile const* source,
  const std::string& config)
{
  // Get appropriate compiler package
  std::string lang = source->GetLanguage();
  std::string compilerPkg = this->GetCompilerPackage(lang);
  
  content += "    buildInputs = [ " << compilerPkg << " ];\n";
  
  // Also update compile command to use correct compiler
  std::string compilerCmd = this->GetCompilerCommand(lang);
  // ... use compilerCmd in buildPhase
}
```

### 3. Support Cross-Compilation
```cpp
// Detect cross-compilation scenario
bool isCrossCompiling = mf->IsOn("CMAKE_CROSSCOMPILING");
if (isCrossCompiling) {
  std::string targetSystem = mf->GetSafeDefinition("CMAKE_SYSTEM_NAME");
  // Map to appropriate cross-compiler package
}
```

## Test Cases

1. **GCC Detection**
   - Set CMAKE_C_COMPILER=gcc
   - Verify generated Nix uses gcc package

2. **Clang Detection**
   - Set CMAKE_C_COMPILER=clang
   - Verify generated Nix uses clang package

3. **Mixed Compilers**
   - C files with gcc, C++ files with clang
   - Verify correct compiler per source file

4. **Cross-Compilation**
   - Set up ARM cross-compilation
   - Verify correct cross-compiler package

## Acceptance Criteria

- [ ] Compiler auto-detection works for gcc and clang
- [ ] CMAKE_C_COMPILER and CMAKE_CXX_COMPILER are respected
- [ ] Per-language compiler selection works
- [ ] Cross-compilation scenarios are handled
- [ ] Tests pass with different compilers

## Related Files

- `Source/cmNixTargetGenerator.cxx` - Main implementation
- `Source/cmGlobalNixGenerator.cxx` - May need updates for global compiler detection
- Test projects in test_multifile/, test_headers/, etc.