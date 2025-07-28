/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */
#pragma once

#include <string>

class cmGeneratorTarget;
class cmMakefile;
class cmGlobalGenerator;

/**
 * @brief Utility class for handling build configuration logic for the Nix generator
 * 
 * This class centralizes build configuration handling to reduce coupling
 * and provide a consistent interface for configuration-related operations.
 */
class cmNixBuildConfiguration
{
public:
  /**
   * @brief Get the build configuration for a target
   * @param target The target to get configuration for (can be nullptr)
   * @param globalGen The global generator for fallback configuration lookup
   * @return The build configuration (Debug, Release, etc.)
   */
  static std::string GetBuildConfiguration(cmGeneratorTarget* target,
                                            const cmGlobalGenerator* globalGen);

  /**
   * @brief Get configuration-specific compile flags
   * @param config The build configuration
   * @return Configuration-specific compiler flags
   */
  static std::string GetConfigurationFlags(const std::string& config);

  /**
   * @brief Check if a configuration is optimized
   * @param config The build configuration
   * @return True if the configuration enables optimizations
   */
  static bool IsOptimizedConfiguration(const std::string& config);

  /**
   * @brief Check if a configuration includes debug info
   * @param config The build configuration
   * @return True if the configuration includes debug symbols
   */
  static bool HasDebugInfo(const std::string& config);

  /**
   * @brief Get the default configuration if none is specified
   * @return The default build configuration
   */
  static std::string GetDefaultConfiguration();

private:
  // Default configuration when none is specified
  static constexpr const char* DEFAULT_CONFIG = "Release";
};