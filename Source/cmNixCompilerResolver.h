/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */
#pragma once

#include <string>
#include <unordered_map>
#include <mutex>

class cmake;

/**
 * @brief Utility class for resolving compiler information for Nix generator
 * 
 * This class centralizes compiler detection logic to avoid duplication
 * and provides a consistent interface for determining compiler packages
 * and commands based on language and configuration.
 */
class cmNixCompilerResolver
{
public:
  explicit cmNixCompilerResolver(cmake* cm);
  ~cmNixCompilerResolver() = default;

  /**
   * @brief Get the Nix package name for a given language compiler
   * @param lang The programming language (C, CXX, Fortran, CUDA, etc.)
   * @return The Nix package name (e.g., "gcc", "clang", "gfortran")
   */
  std::string GetCompilerPackage(const std::string& lang);

  /**
   * @brief Get the compiler command for a given language
   * @param lang The programming language
   * @return The compiler command (e.g., "gcc", "g++", "clang++")
   */
  std::string GetCompilerCommand(const std::string& lang);

  /**
   * @brief Check if a compiler supports cross-compilation
   * @param lang The programming language
   * @return True if cross-compilation is supported
   */
  bool SupportsCrossCompilation(const std::string& lang) const;

  /**
   * @brief Clear all cached compiler information
   */
  void ClearCache();

private:
  struct CompilerInfo {
    std::string package;
    std::string command;
    bool supportsCrossCompile;
  };

  /**
   * @brief Detect compiler information for a language
   * @param lang The programming language
   * @return CompilerInfo structure with detected information
   */
  CompilerInfo DetectCompiler(const std::string& lang) const;

  /**
   * @brief Get compiler ID from CMake cache
   * @param lang The programming language
   * @return Compiler ID string (e.g., "GNU", "Clang", "Intel")
   */
  std::string GetCompilerId(const std::string& lang) const;

  /**
   * @brief Get compiler path from CMake cache
   * @param lang The programming language
   * @return Full path to the compiler executable
   */
  std::string GetCompilerPath(const std::string& lang) const;

  /**
   * @brief Check for user-specified override variable
   * @param lang The programming language
   * @param varSuffix Variable suffix (e.g., "_PACKAGE", "_COMMAND")
   * @return Override value if set, empty string otherwise
   */
  std::string GetUserOverride(const std::string& lang, const std::string& varSuffix) const;

  cmake* CMakeInstance;
  mutable std::unordered_map<std::string, CompilerInfo> CompilerCache;
  mutable std::mutex CacheMutex;

  // Default compiler mappings
  static const std::unordered_map<std::string, std::string> CompilerIdToPackage;
  static const std::unordered_map<std::string, std::string> DefaultCommands;
};