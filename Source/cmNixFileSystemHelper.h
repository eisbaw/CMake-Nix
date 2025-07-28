/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */
#pragma once

#include <string>
#include <vector>

class cmake;

/**
 * @brief Utility class for file system operations in the Nix generator
 * 
 * This class centralizes file system path handling, validation, and
 * system path detection to reduce coupling and improve testability.
 */
class cmNixFileSystemHelper
{
public:
  explicit cmNixFileSystemHelper(cmake* cm);
  ~cmNixFileSystemHelper() = default;

  /**
   * @brief Check if a path is a system path (e.g., /usr, /nix/store)
   * @param path The path to check
   * @return True if the path is a system path
   */
  bool IsSystemPath(const std::string& path) const;

  /**
   * @brief Check if a path is external to the project
   * @param path The path to check
   * @param projectDir The project directory
   * @param buildDir The build directory
   * @return True if the path is external to the project
   */
  bool IsExternalPath(const std::string& path,
                      const std::string& projectDir,
                      const std::string& buildDir) const;

  /**
   * @brief Validate path security for source files
   * @param path The path to validate
   * @param projectDir The project directory
   * @param buildDir The build directory
   * @param errorMessage Output parameter for error message
   * @return True if the path is valid, false otherwise
   */
  bool ValidatePathSecurity(const std::string& path,
                            const std::string& projectDir,
                            const std::string& buildDir,
                            std::string& errorMessage) const;

  /**
   * @brief Normalize a path for use in Nix expressions
   * @param path The path to normalize
   * @return Normalized path
   */
  std::string NormalizePath(const std::string& path) const;

  /**
   * @brief Get relative path with proper handling
   * @param from The base directory
   * @param to The target path
   * @return Relative path or empty string if not possible
   */
  std::string GetRelativePath(const std::string& from,
                               const std::string& to) const;

  /**
   * @brief Check if a path is in the Nix store
   * @param path The path to check
   * @return True if the path is in the Nix store
   */
  bool IsNixStorePath(const std::string& path) const;

  /**
   * @brief Get the list of system path prefixes
   * @return Vector of system path prefixes
   */
  std::vector<std::string> GetSystemPathPrefixes() const;

private:
  cmake* CMakeInstance;
  mutable std::vector<std::string> CachedSystemPaths;
  mutable bool SystemPathsCached = false;

  /**
   * @brief Load system paths from configuration or defaults
   */
  void LoadSystemPaths() const;
};