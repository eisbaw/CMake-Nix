/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */
#pragma once

#include "cmConfigure.h" // IWYU pragma: keep

#include <iosfwd>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "cmGlobalCommonGenerator.h"
#include "cmGlobalGeneratorFactory.h"

class cmGeneratorTarget;
class cmLocalGenerator;
class cmMakefile;
class cmake;

/**
 * \class cmGlobalNixGenerator
 * \brief Write Nix expressions for building C/C++ projects.
 *
 * The Nix generator produces fine-grained Nix derivations that maximize
 * build parallelism and caching efficiency. It generates one derivation
 * per translation unit and separate derivations for linking.
 */
class cmGlobalNixGenerator : public cmGlobalCommonGenerator
{
public:
  cmGlobalNixGenerator(cmake* cm);
  
  static std::unique_ptr<cmGlobalGeneratorFactory> NewFactory()
  {
    return std::unique_ptr<cmGlobalGeneratorFactory>(
      new cmGlobalGeneratorSimpleFactory<cmGlobalNixGenerator>());
  }

  std::unique_ptr<cmLocalGenerator> CreateLocalGenerator(
    cmMakefile* mf) override;

  std::string GetName() const override
  {
    return cmGlobalNixGenerator::GetActualName();
  }
  
  static std::string GetActualName() { return "Nix"; }

  /**
   * Utilized by the generator factory to determine if this generator
   * supports toolsets.
   */
  static bool SupportsToolset() { return false; }

  /**
   * Utilized by the generator factory to determine if this generator
   * supports platforms.
   */
  static bool SupportsPlatform() { return false; }

  /** Get the documentation entry for this generator.  */
  static cmDocumentationEntry GetDocumentation();

  /**
   * Generate the all required files for building this project/tree.
   */
  void Generate() override;

  std::vector<GeneratedMakeCommand> GenerateBuildCommand(
    std::string const& makeProgram, std::string const& projectName,
    std::string const& projectDir, std::vector<std::string> const& targetNames,
    std::string const& config, int jobs, bool verbose,
    cmBuildOptions const& buildOptions = cmBuildOptions(),
    std::vector<std::string> const& makeOptions =
      std::vector<std::string>()) override;

protected:
  void WriteNixFile();
  void WriteDerivations();
  void WritePerTranslationUnitDerivations();
  void WriteLinkingDerivations();

private:
  std::string GetDerivationName(std::string const& targetName, 
                               std::string const& sourceFile = "") const;
  std::vector<std::string> GetSourceDependencies(std::string const& sourceFile) const;
}; 