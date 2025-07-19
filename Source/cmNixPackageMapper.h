/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */
#pragma once

#include <map>
#include <string>

/**
 * \class cmNixPackageMapper
 * \brief Maps CMake packages and imported targets to Nix packages.
 *
 * This class provides mapping from CMake's find_package() results
 * and imported targets to appropriate Nix packages.
 */
class cmNixPackageMapper
{
public:
  /**
   * Get Nix package name for a CMake package
   */
  std::string GetNixPackage(const std::string& cmakePackage) const;
  
  /**
   * Get Nix package name for an imported target (e.g., ZLIB::ZLIB -> zlib)
   */
  std::string GetNixPackageForTarget(const std::string& importedTarget) const;
  
  /**
   * Get link flags for an imported target
   */
  std::string GetLinkFlags(const std::string& importedTarget) const;
  
private:
  /**
   * Built-in package mapping
   */
  static const std::map<std::string, std::string> PackageMap;
  static const std::map<std::string, std::string> TargetMap;
  static const std::map<std::string, std::string> LinkFlagsMap;
};