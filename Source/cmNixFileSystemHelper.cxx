/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */
#include "cmNixFileSystemHelper.h"

#include "cmExpandedCommandArgument.h" // cmExpandList
#include "cmNixPathUtils.h"
#include "cmSystemTools.h"
#include "cmValue.h"
#include "cmake.h"

cmNixFileSystemHelper::cmNixFileSystemHelper(cmake* cm)
  : CMakeInstance(cm)
{
}

bool cmNixFileSystemHelper::IsSystemPath(const std::string& path) const
{
  std::lock_guard<std::mutex> lock(this->CacheMutex);
  
  if (!this->SystemPathsCached) {
    this->LoadSystemPaths();
  }

  // Check against cached system paths
  for (const auto& systemPath : this->CachedSystemPaths) {
    if (cmSystemTools::IsSubDirectory(path, systemPath)) {
      return true;
    }
  }

  // Also consider CMake's own modules directory as a system path
  std::string cmakeRoot = cmSystemTools::GetCMakeRoot();
  if (!cmakeRoot.empty() && cmSystemTools::IsSubDirectory(path, cmakeRoot)) {
    return true;
  }

  return false;
}

bool cmNixFileSystemHelper::IsExternalPath(const std::string& path,
                                            const std::string& projectDir,
                                            const std::string& buildDir) const
{
  // Normalize all paths
  std::string normalizedPath = cmSystemTools::CollapseFullPath(path);
  std::string normalizedProjectDir = cmSystemTools::CollapseFullPath(projectDir);
  std::string normalizedBuildDir = cmSystemTools::CollapseFullPath(buildDir);

  // Check if path is inside project or build directory
  if (cmSystemTools::IsSubDirectory(normalizedPath, normalizedProjectDir) ||
      cmSystemTools::IsSubDirectory(normalizedPath, normalizedBuildDir)) {
    return false;
  }

  // Path is external
  return true;
}

bool cmNixFileSystemHelper::ValidatePathSecurity(const std::string& path,
                                                  const std::string& projectDir,
                                                  const std::string& buildDir,
                                                  std::string& errorMessage) const
{
  // Check for dangerous path patterns
  if (path.find("..") != std::string::npos) {
    // Normalize the path to resolve any .. segments
    std::string normalizedPath = cmSystemTools::CollapseFullPath(path);
    
    // After normalization, check if it's still within project bounds
    if (!cmSystemTools::IsSubDirectory(normalizedPath, projectDir) &&
        !cmSystemTools::IsSubDirectory(normalizedPath, buildDir) &&
        !this->IsSystemPath(normalizedPath)) {
      errorMessage = "Path traversal detected: " + path;
      return false;
    }
  }

  // Check for symlinks that might escape the project
  std::string resolvedPath = cmSystemTools::GetRealPath(path);
  std::string resolvedProjectDir = cmSystemTools::GetRealPath(projectDir);
  
  // Check if resolved path is outside project directory (unless it's a system file)
  if (!cmSystemTools::IsSubDirectory(resolvedPath, resolvedProjectDir) &&
      !this->IsSystemPath(resolvedPath)) {
    // Check if it's in the CMake build directory
    if (!cmSystemTools::IsSubDirectory(resolvedPath, buildDir)) {
      errorMessage = "Source file path is outside project directory: " + path;
      // This is a warning for CMake internal files (like ABI tests), not an error
      return true; // Still valid, but worth noting
    }
  }

  errorMessage.clear();
  return true;
}

std::string cmNixFileSystemHelper::NormalizePath(const std::string& path) const
{
  return cmSystemTools::CollapseFullPath(path);
}

std::string cmNixFileSystemHelper::GetRelativePath(const std::string& from,
                                                    const std::string& to) const
{
  // Use CMake's built-in relative path computation
  std::string relPath = cmSystemTools::RelativePath(from, to);
  
  // Check if the result is valid (not starting with ..)
  if (!relPath.empty() && !cmNixPathUtils::IsPathOutsideTree(relPath)) {
    return relPath;
  }
  
  return "";
}

bool cmNixFileSystemHelper::IsNixStorePath(const std::string& path) const
{
  return cmSystemTools::IsSubDirectory(path, "/nix/store");
}

std::vector<std::string> cmNixFileSystemHelper::GetSystemPathPrefixes() const
{
  std::lock_guard<std::mutex> lock(this->CacheMutex);
  
  if (!this->SystemPathsCached) {
    this->LoadSystemPaths();
  }
  return this->CachedSystemPaths;
}

void cmNixFileSystemHelper::LoadSystemPaths() const
{
  this->CachedSystemPaths.clear();
  
  // Check for CMAKE_NIX_SYSTEM_PATH_PREFIXES variable
  cmValue systemPaths = this->CMakeInstance->GetCacheDefinition("CMAKE_NIX_SYSTEM_PATH_PREFIXES");
  
  if (systemPaths && !systemPaths->empty()) {
    // User-defined system path prefixes (semicolon-separated)
    cmExpandList(*systemPaths, this->CachedSystemPaths);
  } else {
    // Default system paths
    this->CachedSystemPaths = {
      "/usr",
      "/nix/store",
      "/opt",
      "/usr/local",
      "/System",  // macOS
      "/Library"  // macOS
    };
  }
  
  this->SystemPathsCached = true;
}