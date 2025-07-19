# Issue #003: find_package() Integration

**Priority:** HIGH  
**Status:** Not Started  
**Category:** External Dependencies  
**Estimated Effort:** 3-5 days

## Problem Description

Real-world CMake projects heavily rely on `find_package()` to locate and use external dependencies. The Nix generator currently has no support for this, limiting its usefulness to projects without external dependencies.

## Current Behavior

```cmake
find_package(OpenGL REQUIRED)
target_link_libraries(myapp OpenGL::GL)
```

This fails because:
1. No mapping from CMake imported targets to Nix packages
2. No include paths or link flags from found packages
3. No integration with CMake's find modules

## Expected Behavior

The generator should:
1. Map common find_package() results to Nix packages
2. Handle imported targets (OpenGL::GL, Threads::Threads, etc.)
3. Extract include directories and link libraries from found packages
4. Support both system packages and CMake-provided find modules

## Implementation Plan

### 1. Create Package Mapping Infrastructure
```cpp
class cmNixPackageMapper {
public:
  // Map CMake package name to Nix package
  std::string GetNixPackage(const std::string& cmakePackage) {
    static std::map<std::string, std::string> packageMap = {
      {"OpenGL", "libGL"},
      {"GLUT", "freeglut"},
      {"X11", "xorg.libX11"},
      {"Threads", ""},  // Built into gcc/clang
      {"PNG", "libpng"},
      {"JPEG", "libjpeg"},
      {"ZLIB", "zlib"},
      {"CURL", "curl"},
      {"OpenSSL", "openssl"},
      {"Boost", "boost"},
      {"Qt5", "qt5"},
      {"GTK3", "gtk3"},
      {"SDL2", "SDL2"}
    };
    
    auto it = packageMap.find(cmakePackage);
    return (it != packageMap.end()) ? it->second : "";
  }
  
  // Map imported target to Nix package
  std::string GetNixPackageForTarget(const std::string& importedTarget) {
    // Handle namespaced targets like OpenGL::GL
    size_t pos = importedTarget.find("::");
    if (pos != std::string::npos) {
      std::string package = importedTarget.substr(0, pos);
      return GetNixPackage(package);
    }
    return "";
  }
};
```

### 2. Track find_package() Calls
```cpp
// In cmGlobalNixGenerator
class cmGlobalNixGenerator {
  // Track all packages found via find_package()
  std::set<std::string> FoundPackages;
  cmNixPackageMapper PackageMapper;
  
public:
  void AddFoundPackage(const std::string& package) {
    FoundPackages.insert(package);
  }
  
  std::set<std::string> GetRequiredNixPackages() {
    std::set<std::string> nixPackages;
    for (const auto& pkg : FoundPackages) {
      std::string nixPkg = PackageMapper.GetNixPackage(pkg);
      if (!nixPkg.empty()) {
        nixPackages.insert(nixPkg);
      }
    }
    return nixPackages;
  }
};
```

### 3. Handle Imported Targets in Link Phase
```cpp
void cmNixTargetGenerator::WriteLinkDerivation() {
  // Get link libraries including imported targets
  cmLinkImplementation const* linkImpl = 
    this->GeneratorTarget->GetLinkImplementation(config);
  
  std::set<std::string> nixPackages;
  std::vector<std::string> linkFlags;
  
  for (auto const& lib : linkImpl->Libraries) {
    if (lib.Target) {
      if (lib.Target->IsImported()) {
        // Handle imported target
        std::string nixPkg = this->GetGlobalGenerator()
          ->GetPackageMapper().GetNixPackageForTarget(lib.Target->GetName());
        
        if (!nixPkg.empty()) {
          nixPackages.insert(nixPkg);
        }
        
        // Get actual library name to link
        std::string libName = this->ExtractLibraryName(lib.Target);
        linkFlags.push_back("-l" + libName);
      } else {
        // Handle regular target
        // ... existing code
      }
    } else {
      // Handle raw library names
      linkFlags.push_back("-l" + lib.String);
    }
  }
  
  // Add Nix packages to buildInputs
  content << "    buildInputs = [ gcc";
  for (const auto& pkg : nixPackages) {
    content << " " << pkg;
  }
  content << " ];\n";
}
```

### 4. Extract Include Directories from Packages
```cpp
void cmNixTargetGenerator::WriteObjectDerivation() {
  // Get include directories including from imported targets
  std::vector<std::string> includes = 
    this->GeneratorTarget->GetIncludeDirectories(config, lang);
  
  // Also get from imported targets
  cmLinkImplementation const* linkImpl = 
    this->GeneratorTarget->GetLinkImplementation(config);
    
  for (auto const& lib : linkImpl->Libraries) {
    if (lib.Target && lib.Target->IsImported()) {
      // Imported targets may provide interface include directories
      auto importedIncludes = lib.Target->GetProperty("INTERFACE_INCLUDE_DIRECTORIES");
      // Process and add to include paths
    }
  }
}
```

### 5. Generate Package Config Files
```cpp
// For packages not in standard mapping, generate custom Nix expressions
void cmGlobalNixGenerator::GeneratePackageConfig(const std::string& package) {
  if (PackageMapper.GetNixPackage(package).empty()) {
    // Generate pkg_<package>.nix with custom lookup logic
    std::string filename = "pkg_" + package + ".nix";
    // ... generate file content
  }
}
```

## Test Cases

1. **Basic find_package**
   ```cmake
   find_package(Threads REQUIRED)
   add_executable(myapp main.c)
   target_link_libraries(myapp Threads::Threads)
   ```

2. **OpenGL Application**
   ```cmake
   find_package(OpenGL REQUIRED)
   find_package(GLUT REQUIRED)
   add_executable(glapp main.c)
   target_link_libraries(glapp OpenGL::GL GLUT::GLUT)
   ```

3. **Multiple Package Dependencies**
   ```cmake
   find_package(PNG REQUIRED)
   find_package(JPEG REQUIRED)
   find_package(ZLIB REQUIRED)
   add_library(imagelib image.c)
   target_link_libraries(imagelib PNG::PNG JPEG::JPEG ZLIB::ZLIB)
   ```

4. **Optional Packages**
   ```cmake
   find_package(Boost COMPONENTS system filesystem)
   if(Boost_FOUND)
     target_link_libraries(myapp Boost::system Boost::filesystem)
   endif()
   ```

## Acceptance Criteria

- [ ] Common packages (OpenGL, Threads, PNG, etc.) work via find_package()
- [ ] Imported targets are correctly mapped to Nix packages
- [ ] Include directories from packages are passed to compilation
- [ ] Link flags are correctly generated for imported targets
- [ ] Optional packages are handled gracefully
- [ ] Custom package mappings can be added easily

## Package Mapping Table (Initial)

| CMake Package | Nix Package | Notes |
|--------------|-------------|-------|
| OpenGL | libGL | Mesa implementation |
| GLUT | freeglut | |
| Threads | (none) | Built into compiler |
| PNG | libpng | |
| JPEG | libjpeg | |
| ZLIB | zlib | |
| OpenSSL | openssl | |
| CURL | curl | |
| X11 | xorg.libX11 | |
| Boost | boost | May need component mapping |
| Qt5 | qt5 | Complex, may need qt5.qtbase etc |

## Future Enhancements

- Support pkg-config integration
- Handle CMake config packages (not just Find modules)
- Support version constraints in find_package()
- Integration with Nix overlays for custom packages

## Related Files

- `Source/cmGlobalNixGenerator.cxx` - Package tracking
- `Source/cmNixTargetGenerator.cxx` - Target generation with imports
- New file: `Source/cmNixPackageMapper.cxx` - Package mapping logic
- Test projects with external dependencies