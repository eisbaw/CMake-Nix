/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */
#pragma once

namespace cmNix {

// Generator names
namespace Generator {
  constexpr const char* NAME = "Nix";
  constexpr const char* DEFAULT_NIX = "default.nix";
}

// Nix commands
namespace Commands {
  constexpr const char* NIX_BUILD = "nix-build";
  constexpr const char* NIX_SHELL = "nix-shell";
  constexpr const char* NIXPKGS_IMPORT = "with import <nixpkgs> {};";
}

// Compiler packages
namespace Compilers {
  constexpr const char* GCC = "gcc";
  constexpr const char* CLANG = "clang";
  constexpr const char* GFORTRAN = "gfortran";
  constexpr const char* NASM = "nasm";
  constexpr const char* YASM = "yasm";
  constexpr const char* CUDA = "cudatoolkit";
}

// Compiler commands
namespace CompilerCommands {
  constexpr const char* GPP = "g++";
  constexpr const char* CLANGPP = "clang++";
  constexpr const char* GFORTRAN = "gfortran";
  constexpr const char* NVCC = "nvcc";
  constexpr const char* NASM = "nasm";
  constexpr const char* YASM = "yasm";
}

// CMake commands
namespace CMake {
  constexpr const char* COMMAND = "cmake";
  constexpr const char* ECHO_FLAG = " -E echo";
  constexpr const char* SCRIPT_FLAG = " -P ";
  constexpr const char* MODULE_PATH = "CMAKE_MODULE_PATH";
}

// File patterns
namespace FilePatterns {
  constexpr const char* LIBTOOL = ".la";
  constexpr const char* STATIC_LIB_PREFIX = "lib";
  constexpr const char* STATIC_LIB_SUFFIX = ".a";
  constexpr const char* SHARED_LIB_PREFIX = "lib";
  constexpr const char* SHARED_LIB_SUFFIX = ".so";
  constexpr const char* OBJECT_FILE_SUFFIX = ".o";
}

// Debug prefixes
namespace Debug {
  constexpr const char* PREFIX = "[NIX-DEBUG]";
  constexpr const char* TRACE_PREFIX = "[NIX-TRACE]";
  constexpr const char* PROFILE_PREFIX = "[NIX-PROFILE]";
}

// Limits
namespace Limits {
  /**
   * Maximum recursion depth for header dependency scanning.
   * This prevents infinite loops when headers include each other cyclically.
   * A depth of 100 is sufficient for even deeply nested header hierarchies.
   * Most real-world projects have header inclusion depths under 20.
   */
  constexpr int MAX_HEADER_RECURSION_DEPTH = 100;
  
  /**
   * Maximum depth for cycle detection in custom command dependencies.
   * This prevents infinite loops when analyzing circular dependencies.
   * A depth of 100 allows for complex dependency chains while preventing
   * stack overflow from true cycles.
   */
  constexpr int MAX_CYCLE_DETECTION_DEPTH = 100;
  
  /**
   * Maximum size for general dependency caches.
   * At 10,000 entries, this can handle large projects while preventing
   * unbounded memory growth. Each entry typically uses ~100-500 bytes,
   * so this limits cache memory to approximately 1-5 MB.
   */
  constexpr int MAX_DEPENDENCY_CACHE_SIZE = 10000;
  
  /**
   * Number of digits to use for hash-based unique suffixes.
   * Using 10,000 provides a range of 0000-9999 for suffixes,
   * giving a very low probability of collisions while keeping
   * derivation names readable.
   */
  constexpr int HASH_SUFFIX_DIGITS = 10000;
}

// Nix derivation attributes
namespace Derivation {
  constexpr const char* NAME = "name";
  constexpr const char* SRC = "src";
  constexpr const char* BUILD_INPUTS = "buildInputs";
  constexpr const char* BUILD_PHASE = "buildPhase";
  constexpr const char* INSTALL_PHASE = "installPhase";
  constexpr const char* OUTPUTS = "outputs";
  constexpr const char* UNPACK_PHASE = "unpackPhase";
}

// System paths
namespace SystemPaths {
  constexpr const char* USR = "/usr/";
  constexpr const char* USR_LOCAL = "/usr/local/";
  constexpr const char* OPT = "/opt/";
  constexpr const char* SYSTEM_LIBRARY = "/System/Library/";
  constexpr const char* LIBRARY_DEVELOPER = "/Library/Developer/";
  constexpr const char* NIX_STORE = "/nix/store/";
  constexpr const char* BIN = "/bin/";
}

// Error messages
namespace ErrorMessages {
  constexpr const char* CIRCULAR_DEPS = "Circular dependency in custom commands detected!";
  constexpr const char* EXTERNAL_PROJECT_WARNING = "ExternalProject_Add or FetchContent detected";
  constexpr const char* MUTABLE_ENV_INCOMPATIBLE = "requires mutable environment features incompatible with Nix";
}

}  // namespace cmNix