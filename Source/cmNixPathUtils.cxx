/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */
#include "cmNixPathUtils.h"

#include <algorithm>

#include "cmSystemTools.h"

std::string cmNixPathUtils::NormalizePathForNix(const std::string& path, 
                                               const std::string& projectDir)
{
  if (path.empty()) {
    return path;
  }

  // First, resolve the path to its real absolute form
  std::string resolvedPath = ResolveToRealPath(path);
  
  // If it's within the project, make it relative
  std::string relativePath = MakeProjectRelative(resolvedPath, projectDir);
  if (!relativePath.empty()) {
    // Ensure it starts with ./ for Nix
    if (relativePath[0] != '.') {
      return "./" + relativePath;
    }
    return relativePath;
  }
  
  // If it's external, return the absolute path for builtins.path
  return resolvedPath;
}

bool cmNixPathUtils::IsExternalPath(const std::string& path, 
                                   const std::string& projectDir)
{
  if (path.empty()) {
    return false;
  }

  // Resolve both paths to their real forms
  std::string resolvedPath = ResolveToRealPath(path);
  std::string resolvedProjectDir = ResolveToRealPath(projectDir);
  
  // Check if the resolved path is under the project directory
  std::string relativePath = cmSystemTools::RelativePath(resolvedProjectDir, resolvedPath);
  
  // If the relative path starts with ../, it's external
  return relativePath.empty() || relativePath.find("../") == 0;
}

std::string cmNixPathUtils::MakeProjectRelative(const std::string& path, 
                                               const std::string& projectDir)
{
  if (path.empty()) {
    return path;
  }

  // Get the relative path
  std::string relativePath = cmSystemTools::RelativePath(projectDir, path);
  
  // If it starts with ../, it's external
  if (relativePath.empty() || relativePath.find("../") == 0) {
    return "";
  }
  
  return relativePath;
}

std::string cmNixPathUtils::ResolveToRealPath(const std::string& path)
{
  if (path.empty()) {
    return path;
  }

  // First collapse the path to resolve .. segments
  std::string collapsedPath = cmSystemTools::CollapseFullPath(path);
  
  // Then get the real path (follows symlinks)
  std::string realPath = cmSystemTools::GetRealPath(collapsedPath);
  
  return realPath.empty() ? collapsedPath : realPath;
}

bool cmNixPathUtils::ValidatePathSecurity(const std::string& path, 
                                         const std::string& projectDir,
                                         std::string& errorMsg)
{
  errorMsg.clear();
  
  if (path.empty()) {
    errorMsg = "Empty path provided";
    return false;
  }

  // Resolve the path to prevent symlink attacks
  std::string resolvedPath = ResolveToRealPath(path);
  std::string resolvedProjectDir = ResolveToRealPath(projectDir);
  
  // Check for null bytes (security issue)
  if (path.find('\0') != std::string::npos) {
    errorMsg = "Path contains null bytes";
    return false;
  }
  
  // Check for shell metacharacters that could be dangerous
  static const char* const dangerousChars = ";|&`$(){}[]<>\\\"'";
  for (const char* p = dangerousChars; *p; ++p) {
    if (path.find(*p) != std::string::npos) {
      errorMsg = "Path contains potentially dangerous character: ";
      errorMsg += *p;
      return false;
    }
  }
  
  // If path should be within project, verify it
  if (!IsExternalPath(path, projectDir)) {
    // Additional check: ensure resolved path is still under project
    if (resolvedPath.find(resolvedProjectDir) != 0) {
      errorMsg = "Path escapes project directory through symlinks";
      return false;
    }
  }
  
  return true;
}

std::string cmNixPathUtils::AbsolutePathToNixExpr(const std::string& path)
{
  if (path.empty() || !cmSystemTools::FileIsFullPath(path)) {
    return path;
  }
  
  // Use builtins.path for absolute paths
  return "(builtins.path { path = \"" + path + "\"; })";
}

bool cmNixPathUtils::IsPathOutsideTree(const std::string& relativePath)
{
  return relativePath.empty() || relativePath.find("../") == 0;
}