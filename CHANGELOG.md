# CMake Nix Backend - Change Log

## Version 1.1.0 (2025-07-28)

### 🚀 Production-Ready Release

All features complete and thoroughly tested. The CMake Nix backend is now production-ready.

### Recent Enhancements (2025-01-26)
- **✅ ExternalProject/FetchContent Detection** - Automatic detection and warning system
- **✅ Skeleton Package Generation** - Automatic skeleton pkg_*.nix file generation
- **✅ Unified Header Derivations** - Shared header derivations for external sources
- **✅ Build Robustness** - Added `mkdir -p` everywhere for output directories
- **✅ Comprehensive Documentation** - Best practices and guidelines added

### Test Suite Status
- **83 test directories** total (14 in main regression suite)
- **100% pass rate** for all tests in `just dev`
- **No critical bugs or code smells** found in comprehensive review
- **Production validated** by building CMake itself

### Additional Language Support
- **Fortran** - Full support for pure and mixed Fortran/C projects
- **Assembly** - Support for ASM and ASM-ATT languages
- **Object Libraries** - Complete OBJECT library support
- **Module Libraries** - MODULE library type for plugins
- **Interface Libraries** - Header-only library support

### Known Limitations (By Design)
- **Unity builds** - Not supported (Nix achieves better parallelism with fine-grained derivations)
- **Precompiled headers** - Incompatible with Nix's pure build model
- **CUDA** - Not yet implemented
- **ExternalProject/FetchContent** - Incompatible with Nix (use find_package() instead)

## Version 1.0.0 (2025-07-19)

### ✅ Complete Implementation

All planned features have been successfully implemented and tested:

### Core Features
- **🎯 Issue #001: Compiler Auto-Detection** - Automatic detection of GCC/Clang compilers
- **📚 Issue #002: Shared Library Support** - Complete shared library creation with versioning
- **🔍 Issue #003: find_package() Integration** - CMake packages mapped to Nix packages 
- **📁 Issue #004: Subdirectory Support** - Full subdirectory project support
- **⚡ Issue #005: Performance Optimization** - Comprehensive caching system
- **🔧 Issue #006: Custom Commands** - Custom command derivation generation
- **📦 Issue #007: Install Rules** - Install derivations with proper directory structure
- **🏗️ Issue #008: Multi-Config Generator** - Support for Debug/Release/RelWithDebInfo/MinSizeRel
- **🧪 Issue #009: Comprehensive Test Suite** - 14 test projects covering all functionality
- **🔄 Issue #010: CMake Self-Hosting** - CMake can build itself using the Nix generator

### Technical Achievements
- **Fine-Grained Derivations**: One derivation per translation unit for maximum parallelism
- **Advanced Caching**: Target generator and library dependency caching for performance
- **Header Dependencies**: Automatic header dependency tracking and include path resolution
- **External Libraries**: Integration with system packages through find_package()
- **Target Dependencies**: Proper linking using Nix string interpolation
- **Configuration Support**: Full support for all CMake build configurations
- **Install Infrastructure**: Complete install rule support with separate derivations

### Test Coverage
Comprehensive test suite includes:
- **Basic Functionality**: Hello world and multi-file projects
- **Header Dependencies**: Both implicit and explicit header dependency tests  
- **Compiler Detection**: Automatic C/C++ compiler detection
- **Shared Libraries**: Library creation, versioning, and linking
- **External Dependencies**: find_package() integration with system libraries
- **Subdirectories**: Multi-directory project support
- **Custom Commands**: Code generation and custom build steps
- **Error Conditions**: Edge cases and error handling
- **Complex Builds**: Advanced build configurations
- **Install Rules**: Installation functionality
- **Multi-Config**: Multiple build configuration support

### Build Status
- ✅ All 14 test projects pass
- ✅ CMake self-hosting successful  
- ✅ Zero known critical issues
- ✅ Production ready

## Previous Versions

### Phase 1 (Initial Implementation)
- Basic generator structure
- Simple C compilation
- Single-file projects only

### Phase 2 (Enhancement)  
- Multi-file project support
- Header dependency tracking
- External library integration

### Phase 3 (Advanced Features)
- Shared library support
- Performance optimizations
- Custom command support

### Phase 4 (Production Ready)
- Install rule support
- Multi-configuration builds
- Comprehensive testing
- Documentation complete