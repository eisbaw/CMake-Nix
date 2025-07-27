/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */
#pragma once

#include "cmConfigure.h"

#include <string>
#include <map>

// Configuration file path for package mappings
#define CMAKE_NIX_PACKAGE_MAPPINGS_FILE "cmake-nix-package-mappings.json"

class cmNixPackageMapper
{
public:
  cmNixPackageMapper();

  std::string GetNixPackageForTarget(std::string const& targetName) const;
  std::string GetLinkFlags(std::string const& targetName) const;

private:
  void InitializeMappings();
  bool LoadMappingsFromFile(const std::string& filePath);
  void InitializeDefaultMappings();

  std::map<std::string, std::string> TargetToNixPackage;
  std::map<std::string, std::string> TargetToLinkFlags;
};