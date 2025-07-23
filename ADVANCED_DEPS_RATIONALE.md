# Advanced Dependency Features and the Nix Backend

## Why Advanced Dependency Features Don't Apply to the Nix Backend

Ninja has several advanced dependency features that optimize build performance:

1. **Order-only Dependencies**: Dependencies that trigger rebuilds but don't affect the command line
2. **Restat**: Re-stat outputs after build to avoid unnecessary downstream rebuilds
3. **Console Pool**: Limits parallel execution of interactive commands

## Nix Backend Approach

These features are not applicable to the Nix backend because:

### 1. Order-only Dependencies
- **Ninja**: Used to ensure files exist without affecting the build command
- **Nix**: All dependencies are content-addressed; if content doesn't change, the derivation isn't rebuilt
- **Result**: Nix automatically provides this behavior through its design

### 2. Restat Feature
- **Ninja**: Re-checks if outputs actually changed to avoid cascading rebuilds
- **Nix**: Content-addressed storage means identical outputs have the same hash
- **Result**: Nix inherently avoids unnecessary rebuilds through content addressing

### 3. Console Pool
- **Ninja**: Limits parallel execution of commands that need terminal access
- **Nix**: Derivations run in isolated environments without terminal access
- **Result**: Interactive commands are not supported in Nix derivations by design

## Conclusion

The Nix backend achieves the same goals as these advanced features through its fundamental design:
- Content-addressed storage eliminates unnecessary rebuilds
- Derivation isolation prevents resource conflicts
- Pure functional approach ensures reproducibility

These Ninja-specific optimizations are solutions to problems that don't exist in the Nix model.