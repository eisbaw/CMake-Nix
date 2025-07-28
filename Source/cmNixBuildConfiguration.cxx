/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */
#include "cmNixBuildConfiguration.h"

#include "cmGeneratorTarget.h"
#include "cmGlobalGenerator.h"
#include "cmLocalGenerator.h"
#include "cmMakefile.h"
#include "cmTarget.h"

std::string cmNixBuildConfiguration::GetBuildConfiguration(cmGeneratorTarget* target,
                                                           const cmGlobalGenerator* globalGen)
{
  if (target) {
    std::string config = target->Target->GetMakefile()->GetSafeDefinition("CMAKE_BUILD_TYPE");
    if (config.empty()) {
      config = DEFAULT_CONFIG;
    }
    return config;
  }
  
  // When no target is provided, check the top-level makefile
  if (globalGen) {
    const auto& localGens = globalGen->GetLocalGenerators();
    if (!localGens.empty()) {
      cmMakefile* mf = localGens[0]->GetMakefile();
      std::string config = mf->GetSafeDefinition("CMAKE_BUILD_TYPE");
      if (config.empty()) {
        config = DEFAULT_CONFIG;
      }
      return config;
    }
  }
  
  // Fallback
  return DEFAULT_CONFIG;
}

std::string cmNixBuildConfiguration::GetConfigurationFlags(const std::string& config)
{
  // Map configuration to standard flags
  if (config == "Debug") {
    return "-g -O0";
  } else if (config == "Release") {
    return "-O3 -DNDEBUG";
  } else if (config == "RelWithDebInfo") {
    return "-O2 -g -DNDEBUG";
  } else if (config == "MinSizeRel") {
    return "-Os -DNDEBUG";
  }
  
  // Default to Release flags for unknown configurations
  return "-O3 -DNDEBUG";
}

bool cmNixBuildConfiguration::IsOptimizedConfiguration(const std::string& config)
{
  return config == "Release" || config == "RelWithDebInfo" || config == "MinSizeRel";
}

bool cmNixBuildConfiguration::HasDebugInfo(const std::string& config)
{
  return config == "Debug" || config == "RelWithDebInfo";
}

std::string cmNixBuildConfiguration::GetDefaultConfiguration()
{
  return DEFAULT_CONFIG;
}