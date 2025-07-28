/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */
#pragma once

#include <string>
#include <vector>
#include <iosfwd>
#include <functional>
#include <memory>

class cmGeneratorTarget;
class cmGeneratedFileStream;

/**
 * @brief Handles generation of Nix install derivations for CMake targets
 *
 * This class is responsible for generating install derivations that copy
 * built artifacts to their final installation locations.
 */
class cmNixInstallRuleGenerator
{
public:
  cmNixInstallRuleGenerator();
  ~cmNixInstallRuleGenerator();

  /**
   * @brief Collects all targets that have install rules
   * @param localGenerators List of local generators to scan for targets
   * @return List of targets that have install rules defined
   */
  std::vector<cmGeneratorTarget*> CollectInstallTargets(
    const std::vector<std::unique_ptr<class cmLocalGenerator>>& localGenerators);

  /**
   * @brief Writes install derivations for all collected targets
   * @param installTargets List of targets with install rules
   * @param nixFileStream Output stream for the Nix file
   * @param buildConfiguration Build configuration (e.g., "Release")
   * @param getDerivationName Function to get derivation name for a target
   */
  void WriteInstallRules(
    const std::vector<cmGeneratorTarget*>& installTargets,
    cmGeneratedFileStream& nixFileStream,
    const std::string& buildConfiguration,
    std::function<std::string(const std::string&)> getDerivationName);

  /**
   * @brief Writes install outputs section for the Nix file
   * @param installTargets List of targets with install rules
   * @param nixFileStream Output stream for the Nix file
   * @param getDerivationName Function to get derivation name for a target
   */
  void WriteInstallOutputs(
    const std::vector<cmGeneratorTarget*>& installTargets,
    cmGeneratedFileStream& nixFileStream,
    std::function<std::string(const std::string&)> getDerivationName);

private:
  /**
   * @brief Gets the library prefix (e.g., "lib" on Unix)
   * @return Library prefix string
   */
  std::string GetLibraryPrefix() const;

  /**
   * @brief Gets the static library extension (e.g., ".a" on Unix)
   * @return Static library extension
   */
  std::string GetStaticLibraryExtension() const;
};