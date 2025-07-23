# Explicit Sources Test

This test verifies that CMAKE_NIX_EXPLICIT_SOURCES=ON works correctly.

When enabled, each compilation unit gets its own source derivation that includes only:
- The source file itself
- Headers it depends on

This provides better build isolation and prevents unnecessary rebuilds when unrelated files change.