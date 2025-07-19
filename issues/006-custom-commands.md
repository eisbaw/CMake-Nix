# Issue #006: Custom Commands and Code Generation

**Priority:** MEDIUM  
**Status:** Not Started  
**Category:** Advanced Features  
**Estimated Effort:** 3-5 days

## Problem Description

Many CMake projects use custom commands for:
- Code generation (protobuf, thrift, etc.)
- Resource processing
- Pre/post build steps
- Custom build tools

The Nix generator needs to support `add_custom_command()` and `add_custom_target()`.

## Current Behavior

Custom commands are ignored, preventing projects with code generation from building.

## Expected Behavior

The generator should:
1. Convert custom commands to Nix derivations
2. Handle command dependencies and outputs
3. Support generator expressions in commands
4. Integrate generated files into build pipeline

## Implementation Plan

### 1. Custom Command Infrastructure
```cpp
class cmNixCustomCommandGenerator {
  cmCustomCommand const* Command;
  cmLocalGenerator* LocalGen;
  
public:
  void Generate(std::ostream& content);
  std::string GetDerivationName() const;
  std::vector<std::string> GetOutputs() const;
  std::vector<std::string> GetDepends() const;
};

void cmNixCustomCommandGenerator::Generate(std::ostream& content) {
  content << "  " << this->GetDerivationName() << " = stdenv.mkDerivation {\n";
  content << "    name = \"custom-command\";\n";
  
  // Add dependencies
  auto depends = this->GetDepends();
  if (!depends.empty()) {
    content << "    buildInputs = [\n";
    for (auto& dep : depends) {
      content << "      " << this->MapToNixDep(dep) << "\n";
    }
    content << "    ];\n";
  }
  
  // Generate build phase from command
  content << "    buildPhase = ''\n";
  
  // Expand command with proper escaping
  std::string cmd = this->Command->GetCommandLines()[0];
  content << "      " << this->EscapeForNix(cmd) << "\n";
  
  // Copy outputs to $out
  for (auto& output : this->GetOutputs()) {
    content << "      cp " << output << " $out/" << output << "\n";
  }
  
  content << "    '';\n";
  content << "  };\n\n";
}
```

### 2. Track Custom Commands in Targets
```cpp
void cmNixTargetGenerator::Generate() {
  // First generate custom commands this target depends on
  std::vector<cmCustomCommand> customCommands = 
    this->GeneratorTarget->GetPreBuildCommands();
  
  for (auto& cmd : customCommands) {
    cmNixCustomCommandGenerator ccGen(&cmd, this->LocalGenerator);
    ccGen.Generate(this->Content);
    
    // Track outputs for use in target
    for (auto& output : ccGen.GetOutputs()) {
      this->CustomCommandOutputs[output] = ccGen.GetDerivationName();
    }
  }
  
  // Include custom command outputs in source list
  // ... existing target generation
}
```

### 3. Handle Generated Sources
```cpp
void cmNixTargetGenerator::WriteObjectDerivations() {
  auto sources = this->GeneratorTarget->GetSourceFiles();
  
  for (auto& source : sources) {
    if (source->GetIsGenerated()) {
      // This source comes from a custom command
      std::string customCmdDrv = this->GetCustomCommandForOutput(source->GetFullPath());
      
      // Add dependency on custom command
      this->AddDependency(customCmdDrv);
      
      // Adjust source path to use custom command output
      // ...
    }
    
    this->WriteObjectDerivation(source);
  }
}
```

### 4. Support add_custom_target()
```cpp
void cmNixTargetGenerator::GenerateCustomTarget() {
  // Custom targets are always built
  content << "  " << this->GetDerivationName() << " = stdenv.mkDerivation {\n";
  content << "    name = \"" << this->GeneratorTarget->GetName() << "\";\n";
  
  // Get commands for this custom target
  auto commands = this->GeneratorTarget->GetCustomCommands();
  
  content << "    buildPhase = ''\n";
  for (auto& cmd : commands) {
    content << "      " << this->ExpandCommand(cmd) << "\n";
  }
  content << "    '';\n";
  
  // Handle dependencies
  auto deps = this->GeneratorTarget->GetUtilities();
  if (!deps.empty()) {
    content << "    deps = [\n";
    for (auto& dep : deps) {
      content << "      " << dep << "\n";
    }
    content << "    ];\n";
  }
  
  content << "  };\n\n";
}
```

### 5. Generator Expression Support
```cpp
std::string cmNixCustomCommandGenerator::ExpandGeneratorExpressions(
  const std::string& input) 
{
  cmGeneratorExpression ge;
  auto cge = ge.Parse(input);
  
  cmGeneratorExpressionContext context(
    this->LocalGen,
    this->Config,
    false,
    nullptr
  );
  
  return cge->Evaluate(&context);
}
```

## Test Cases

1. **Code Generation**
   ```cmake
   add_custom_command(
     OUTPUT generated.c
     COMMAND python ${CMAKE_SOURCE_DIR}/generate.py > generated.c
     DEPENDS generate.py
   )
   add_executable(myapp main.c generated.c)
   ```

2. **Pre-Build Commands**
   ```cmake
   add_custom_command(
     TARGET myapp PRE_BUILD
     COMMAND ${CMAKE_COMMAND} -E copy_directory
       ${CMAKE_SOURCE_DIR}/resources $<TARGET_FILE_DIR:myapp>
   )
   ```

3. **Custom Target**
   ```cmake
   add_custom_target(
     generate_docs
     COMMAND doxygen ${CMAKE_SOURCE_DIR}/Doxyfile
     WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
   )
   ```

4. **Generator Expressions**
   ```cmake
   add_custom_command(
     OUTPUT $<CONFIG>.txt
     COMMAND echo $<CONFIG> > $<CONFIG>.txt
   )
   ```

## Acceptance Criteria

- [ ] add_custom_command(OUTPUT ...) works
- [ ] PRE_BUILD, PRE_LINK, POST_BUILD commands work
- [ ] add_custom_target() works
- [ ] Dependencies between custom commands work
- [ ] Generated sources are handled correctly
- [ ] Generator expressions are expanded
- [ ] WORKING_DIRECTORY is respected

## Challenges

- Nix derivations are more restrictive than shell commands
- Network access is disabled in Nix builds
- Absolute paths need careful handling
- Some commands may need wrapping

## Related Files

- `Source/cmNixTargetGenerator.cxx` - Integration point
- New file: `Source/cmNixCustomCommandGenerator.cxx`
- Test projects with code generation