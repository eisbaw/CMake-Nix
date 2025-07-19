# Issue #002: Shared Library Support

**Priority:** HIGH  
**Status:** Not Started  
**Category:** Core Functionality  
**Estimated Effort:** 2-3 days

## Problem Description

The Nix generator currently only supports static libraries. Shared libraries (.so, .dylib, .dll) are essential for:
- Dynamic linking scenarios
- Plugin architectures
- Reducing binary sizes
- System library compatibility

## Current Behavior

In `cmNixTargetGenerator::Generate()`:
```cpp
case cmStateEnums::SHARED_LIBRARY:
  // TODO: Implement shared library support
  break;
```

Shared library targets are silently ignored.

## Expected Behavior

The generator should:
1. Generate derivations for shared libraries with proper naming
2. Handle library versioning (libfoo.so.1.2.3)
3. Set up proper RPATH/RUNPATH for Nix environments
4. Support both building and linking shared libraries

## Implementation Plan

### 1. Implement Shared Library Generation
```cpp
void cmNixTargetGenerator::GenerateSharedLibrary() {
  std::string targetName = this->GeneratorTarget->GetName();
  
  // Write object file derivations (same as static)
  this->WriteObjectBuildDerivations();
  
  // Write shared library link derivation
  std::ostream& content = this->GetGlobalGenerator()->GetNixContent();
  
  content << "  " << this->GetDerivationName("shared") << " = stdenv.mkDerivation {\n";
  content << "    name = \"lib" << targetName << ".so\";\n";
  content << "    buildInputs = [ gcc ];\n";
  
  // Collect object files
  content << "    objects = [\n";
  for (auto const& source : sources) {
    content << "      " << this->GetObjectDerivationName(source) << "\n";
  }
  content << "    ];\n";
  
  // Build shared library with proper flags
  content << "    buildPhase = ''\n";
  content << "      gcc -shared -fPIC $objects -o $out/lib" << targetName << ".so\n";
  
  // Handle library versioning
  std::string version = this->GeneratorTarget->GetProperty("VERSION");
  if (!version.empty()) {
    content << "      ln -s lib" << targetName << ".so $out/lib" << targetName << ".so." << version << "\n";
  }
  
  content << "    '';\n";
  content << "  };\n\n";
}
```

### 2. Update Object Compilation for PIC
```cpp
void cmNixTargetGenerator::WriteObjectDerivation(
  std::ostream& content,
  cmSourceFile const* source,
  const std::string& config)
{
  // Check if building for shared library
  bool needsPIC = (this->GeneratorTarget->GetType() == cmStateEnums::SHARED_LIBRARY);
  
  // Add -fPIC flag for shared libraries
  if (needsPIC) {
    compileFlags += " -fPIC";
  }
  
  // ... rest of compilation
}
```

### 3. Handle RPATH in Nix Environment
```cpp
// In link phase
content << "    buildPhase = ''\n";
content << "      gcc -shared -fPIC $objects -o $out/lib" << targetName << ".so \\\n";
content << "        -Wl,-rpath,$out/lib \\\n";  // Nix-specific RPATH
content << "        -Wl,-soname,lib" << targetName << ".so\n";
content << "    '';\n";
```

### 4. Support Linking Against Shared Libraries
```cpp
void cmNixTargetGenerator::WriteLinkDerivation() {
  // Detect shared library dependencies
  for (auto const& lib : linkLibraries) {
    if (lib.Target && lib.Target->GetType() == cmStateEnums::SHARED_LIBRARY) {
      // Add to buildInputs and link flags
      content << "      -L${" << lib.Target->GetName() << "_shared}/lib \\\n";
      content << "      -l" << lib.Target->GetName() << " \\\n";
    }
  }
}
```

## Test Cases

1. **Basic Shared Library**
   ```cmake
   add_library(mylib SHARED src/lib.c)
   ```
   - Verify .so file is created
   - Check -fPIC is used

2. **Versioned Library**
   ```cmake
   add_library(mylib SHARED src/lib.c)
   set_target_properties(mylib PROPERTIES VERSION 1.2.3 SOVERSION 1)
   ```
   - Verify symlinks are created correctly

3. **Executable Linking Shared Library**
   ```cmake
   add_library(mylib SHARED src/lib.c)
   add_executable(myapp src/main.c)
   target_link_libraries(myapp mylib)
   ```
   - Verify RPATH is set correctly
   - Check executable runs with shared library

4. **Mixed Static/Shared**
   - Project with both static and shared libraries
   - Verify correct linking behavior

## Acceptance Criteria

- [ ] Shared libraries build successfully
- [ ] -fPIC flag is applied to object files for shared libraries
- [ ] Library versioning works (VERSION and SOVERSION properties)
- [ ] RPATH/RUNPATH is set correctly for Nix
- [ ] Executables can link and run with shared libraries
- [ ] Mixed static/shared library projects work

## Implementation Notes

- Nix handles RPATH differently than standard Linux
- Must use $out paths for RPATH to work in Nix store
- Consider supporting MODULE libraries (plugins) as well
- Windows DLL support is out of scope for initial implementation

## Related Files

- `Source/cmNixTargetGenerator.cxx` - Main implementation
- `Source/cmNixTargetGenerator.h` - Add GenerateSharedLibrary method
- Test projects need shared library examples