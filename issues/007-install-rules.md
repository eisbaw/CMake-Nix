# Issue #007: Install Rules and Packaging

**Priority:** MEDIUM  
**Status:** Not Started  
**Category:** Installation  
**Estimated Effort:** 2-3 days

## Problem Description

CMake projects use install() commands to define installation layout. The Nix generator needs to support installation rules to create proper Nix packages.

## Current Behavior

Install commands are ignored. There's no way to create installable Nix packages from CMake projects.

## Expected Behavior

The generator should:
1. Process install() commands
2. Generate Nix expressions with proper install phases
3. Handle different installation components
4. Support CMAKE_INSTALL_PREFIX in Nix context

## Implementation Plan

### 1. Track Install Rules
```cpp
class cmNixInstallCollector {
  struct InstallRule {
    cmInstallGenerator* Generator;
    std::string Component;
    std::string Destination;
  };
  
  std::vector<InstallRule> Rules;
  
public:
  void AddRule(cmInstallGenerator* gen);
  void GenerateInstallPhase(std::ostream& content);
};

void cmGlobalNixGenerator::AddInstallRule(cmInstallGenerator* gen) {
  this->InstallCollector.AddRule(gen);
}
```

### 2. Generate Install Phase
```cpp
void cmNixInstallCollector::GenerateInstallPhase(std::ostream& content) {
  if (this->Rules.empty()) {
    return;
  }
  
  content << "    installPhase = ''\n";
  content << "      mkdir -p $out\n";
  
  for (auto& rule : this->Rules) {
    this->GenerateInstallCommand(content, rule);
  }
  
  content << "    '';\n";
}

void cmNixInstallCollector::GenerateInstallCommand(
  std::ostream& content,
  const InstallRule& rule) 
{
  // Handle different install generator types
  if (auto* targetGen = dynamic_cast<cmInstallTargetGenerator*>(rule.Generator)) {
    // Install target outputs
    auto* target = targetGen->GetTarget();
    std::string dest = this->GetNixDestination(rule.Destination);
    
    content << "      mkdir -p $out/" << dest << "\n";
    content << "      cp ${" << target->GetName() << "} ";
    content << "$out/" << dest << "/" << target->GetName() << "\n";
    
  } else if (auto* filesGen = dynamic_cast<cmInstallFilesGenerator*>(rule.Generator)) {
    // Install files
    for (auto& file : filesGen->GetFiles()) {
      std::string dest = this->GetNixDestination(rule.Destination);
      content << "      mkdir -p $out/" << dest << "\n";
      content << "      cp " << file << " $out/" << dest << "/\n";
    }
    
  } else if (auto* dirGen = dynamic_cast<cmInstallDirectoryGenerator*>(rule.Generator)) {
    // Install directories
    for (auto& dir : dirGen->GetDirectories()) {
      std::string dest = this->GetNixDestination(rule.Destination);
      content << "      cp -r " << dir << " $out/" << dest << "/\n";
    }
  }
}
```

### 3. Update Target Derivations
```cpp
void cmNixTargetGenerator::WriteLinkDerivation() {
  // ... existing link phase ...
  
  // Add install phase if target has install rules
  if (this->HasInstallRules()) {
    auto* installCollector = this->GlobalGenerator->GetInstallCollector();
    installCollector->GenerateInstallPhase(content);
  }
  
  content << "  };\n\n";
}
```

### 4. Support Installation Components
```cpp
void cmGlobalNixGenerator::WriteNixFile() {
  // Generate separate outputs for different components
  content << "{\n";
  
  // Default output includes everything
  content << "  default = stdenv.mkDerivation {\n";
  content << "    name = \"" << this->GetProjectName() << "\";\n";
  // ... generate full build ...
  content << "  };\n";
  
  // Generate per-component outputs
  auto components = this->InstallCollector.GetComponents();
  for (auto& comp : components) {
    content << "  \n";
    content << "  " << comp << " = stdenv.mkDerivation {\n";
    content << "    name = \"" << this->GetProjectName() << "-" << comp << "\";\n";
    // ... generate component-specific install ...
    content << "  };\n";
  }
  
  content << "}\n";
}
```

### 5. Handle Install Directories
```cpp
std::string cmNixInstallCollector::GetNixDestination(const std::string& dest) {
  // Convert CMake install destinations to Nix conventions
  static std::map<std::string, std::string> destMap = {
    {"bin", "bin"},
    {"sbin", "sbin"},
    {"lib", "lib"},
    {"lib64", "lib"},
    {"include", "include"},
    {"share", "share"},
    {"etc", "etc"},
    {"var", "var"}
  };
  
  // Handle absolute paths
  if (cmSystemTools::FileIsFullPath(dest)) {
    // Strip CMAKE_INSTALL_PREFIX if present
    std::string prefix = this->Makefile->GetSafeDefinition("CMAKE_INSTALL_PREFIX");
    if (cmHasPrefix(dest, prefix)) {
      return dest.substr(prefix.length() + 1);
    }
  }
  
  // Map or return as-is
  auto it = destMap.find(dest);
  return (it != destMap.end()) ? it->second : dest;
}
```

## Test Cases

1. **Basic Installation**
   ```cmake
   add_executable(myapp main.c)
   install(TARGETS myapp DESTINATION bin)
   ```

2. **Library Installation**
   ```cmake
   add_library(mylib SHARED lib.c)
   install(TARGETS mylib
     LIBRARY DESTINATION lib
     ARCHIVE DESTINATION lib
     INCLUDES DESTINATION include
   )
   install(FILES mylib.h DESTINATION include)
   ```

3. **Component Installation**
   ```cmake
   install(TARGETS app1 DESTINATION bin COMPONENT applications)
   install(TARGETS app2 DESTINATION bin COMPONENT utilities)
   install(FILES README.md DESTINATION share/doc COMPONENT documentation)
   ```

4. **Directory Installation**
   ```cmake
   install(DIRECTORY resources/ DESTINATION share/myapp)
   install(PROGRAMS scripts/helper.sh DESTINATION bin)
   ```

## Acceptance Criteria

- [ ] install(TARGETS ...) works for executables and libraries
- [ ] install(FILES ...) and install(PROGRAMS ...) work
- [ ] install(DIRECTORY ...) works
- [ ] Installation components generate separate outputs
- [ ] Destinations are mapped correctly to Nix conventions
- [ ] Permissions are preserved (executable bits)
- [ ] Generated packages are valid Nix packages

## Nix Package Structure

Generated Nix packages should follow standard layout:
```
$out/
  bin/        # Executables
  lib/        # Libraries
  include/    # Headers
  share/      # Data files
    doc/      # Documentation
    man/      # Man pages
```

## Future Enhancements

- Support for CMake's install(EXPORT ...) 
- Integration with Nix multiple outputs
- Support for post-install scripts (wrapped appropriately)
- CPack integration for more complex packaging

## Related Files

- `Source/cmGlobalNixGenerator.cxx` - Install rule collection
- `Source/cmNixTargetGenerator.cxx` - Per-target install integration
- New file: `Source/cmNixInstallCollector.cxx` - Install rule processing