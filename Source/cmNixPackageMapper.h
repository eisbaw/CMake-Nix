/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */
#pragma once

#include "cmConfigure.h"

#include <string>
#include <map>
#include <mutex>
#include <memory>

// Configuration file path for package mappings
#define CMAKE_NIX_PACKAGE_MAPPINGS_FILE "cmake-nix-package-mappings.json"

class cmNixPackageMapper
{
public:
  // Singleton instance for thread-safe access
  static const cmNixPackageMapper& GetInstance();

  std::string GetNixPackageForTarget(std::string const& targetName) const;
  std::string GetLinkFlags(std::string const& targetName) const;

private:
  cmNixPackageMapper();
  void InitializeMappings();
  bool LoadMappingsFromFile(const std::string& filePath);
  void InitializeDefaultMappings();

  std::map<std::string, std::string> TargetToNixPackage;
  std::map<std::string, std::string> TargetToLinkFlags;
  
  static std::unique_ptr<cmNixPackageMapper> Instance;
  static std::mutex InstanceMutex;
};