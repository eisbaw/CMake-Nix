# Test: Special Characters in Target Names

This test demonstrates that the CMake Nix generator has issues with certain special characters in target names.

## Results

### CMake Input Validation
CMake itself rejects most special characters including:
- @ = ( ) [ ] , ; space ! ? * $ # % & ' ` ~ | \ : < >

This provides good protection against injection attacks.

### Nix Generator Issues Found

The Nix generator creates invalid Nix syntax for targets containing:
1. **Dots (.)** - Creates `my.test.app_..._o` which is invalid syntax
2. **Plus (+)** - Creates `my+app_..._o` which is invalid syntax  
3. **Numeric prefixes** - Creates `123_app_..._o` which is invalid (Nix identifiers can't start with numbers)
4. **All numeric names** - Creates `456789_..._o` which is invalid
5. **Dashes (-)** - Creates `my-test-app_..._o` which needs quoting

### Example Error
```
error: syntax error, unexpected '+', expecting '.' or '='
       at /path/to/default.nix:104:5:
          104|   my+app_test_special_characters_main_c_o = stdenv.mkDerivation {
```

## Recommendation

The Nix generator should sanitize target names when creating Nix identifiers by:
1. Replacing dots, plus signs, and dashes with underscores
2. Prefixing numeric names with a letter (e.g., `t_123_app`)
3. Properly escaping or transforming any other problematic characters

This would ensure valid Nix syntax while preserving the original target names in the final attribute set (which already uses proper quoting).