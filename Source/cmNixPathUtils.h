/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */
#ifndef cmNixPathUtils_h
#define cmNixPathUtils_h

#include <string>
#include <vector>

#include "cmConfigure.h" // IWYU pragma: keep

class cmNixPathUtils
{
public:
  /**
   * Normalize a path for use in Nix expressions.
   * Handles absolute paths, relative paths, and paths with .. segments.
   * 
   * @param path The path to normalize
   * @param projectDir The project root directory
   * @return Normalized path suitable for Nix
   */
  static std::string NormalizePathForNix(const std::string& path, 
                                        const std::string& projectDir);

  /**
   * Check if a path is external to the project.
   * A path is external if it's outside the project directory tree.
   * 
   * @param path The path to check
   * @param projectDir The project root directory
   * @return true if the path is external to the project
   */
  static bool IsExternalPath(const std::string& path, 
                            const std::string& projectDir);

  /**
   * Make a path relative to the project directory.
   * Handles both absolute and relative paths.
   * 
   * @param path The path to make relative
   * @param projectDir The project root directory
   * @return Relative path, or empty string if path is external
   */
  static std::string MakeProjectRelative(const std::string& path, 
                                        const std::string& projectDir);

  /**
   * Resolve a path to its real absolute path.
   * Follows symlinks and resolves .. segments.
   * 
   * @param path The path to resolve
   * @return Fully resolved absolute path
   */
  static std::string ResolveToRealPath(const std::string& path);

  /**
   * Validate a path for security.
   * Checks for path traversal attempts and other security issues.
   * 
   * @param path The path to validate
   * @param projectDir The project root directory
   * @param errorMsg Output parameter for error message if validation fails
   * @return true if the path is valid and safe
   */
  static bool ValidatePathSecurity(const std::string& path, 
                                  const std::string& projectDir,
                                  std::string& errorMsg);

  /**
   * Convert an absolute path to a Nix store path reference.
   * Uses builtins.path for absolute paths outside the project.
   * 
   * @param path The absolute path to convert
   * @return Nix expression for the path
   */
  static std::string AbsolutePathToNixExpr(const std::string& path);

  /**
   * Check if a relative path goes outside the tree (starts with ../).
   * This is a convenience function for a common pattern.
   * 
   * @param relativePath The relative path to check
   * @return true if the path starts with ../
   */
  static bool IsPathOutsideTree(const std::string& relativePath);
};

#endif