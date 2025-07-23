# Unity Builds and the Nix Backend

## Why Unity Builds Don't Apply to the Nix Backend

Unity builds are a compilation optimization technique where multiple source files are combined into a single compilation unit. This is done to:

1. Reduce compilation time by parsing headers only once
2. Reduce linker work by having fewer object files
3. Enable better compiler optimizations across translation units

## Nix Backend Philosophy

The Nix backend takes the opposite approach for good reasons:

1. **Maximum Parallelism**: Each source file compiles in its own derivation, allowing unlimited parallel compilation across machines
2. **Perfect Caching**: When a single source file changes, only that file needs recompilation
3. **Content-Addressed Storage**: Nix automatically detects when files haven't changed and reuses cached results
4. **Distributed Builds**: Individual derivations can be built on different machines

## Technical Incompatibility

Unity builds would:
- Force multiple files to rebuild when only one changes
- Reduce parallelism by combining independent compilation units
- Break Nix's fine-grained dependency tracking
- Negate the primary benefits of using Nix for builds

## Conclusion

Unity builds are incompatible with the Nix backend's design goals. The Nix backend achieves faster builds through parallelism and caching rather than reducing compilation units. Therefore, the UNITY_BUILD property should be ignored by the Nix generator.

## Implementation Note

The Nix generator should emit a warning if UNITY_BUILD is enabled on a target, explaining that unity builds are not supported and will be ignored.