/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */
#pragma once

#include "cmConfigure.h" // IWYU pragma: keep

#include <memory>
#include <string>
#include <vector>

#include "cmLocalCommonGenerator.h"

class cmGeneratorTarget;
class cmGlobalGenerator;
class cmGlobalNixGenerator;
class cmMakefile;
class cmRulePlaceholderExpander;
class cmake;

/**
 * \class cmLocalNixGenerator
 * \brief Write local Nix derivations.
 *
 * cmLocalNixGenerator produces local Nix derivations from its
 * member Makefile.
 */
class cmLocalNixGenerator : public cmLocalCommonGenerator
{
public:
  cmLocalNixGenerator(cmGlobalGenerator* gg, cmMakefile* mf);

  void Generate() override;

  void GenerateTargetManifest();

  std::unique_ptr<cmRulePlaceholderExpander> CreateRulePlaceholderExpander(
    cmBuildStep buildStep = cmBuildStep::Compile) const override;

  cmGlobalNixGenerator const* GetGlobalNixGenerator() const;
  cmGlobalNixGenerator* GetGlobalNixGenerator();

protected:
  void WriteLocalTargets();
  void WriteTargetDerivations(cmGeneratorTarget* target);

private:
  void WriteCompileDerivations(cmGeneratorTarget* target);
  void WriteLinkDerivation(cmGeneratorTarget* target);
}; 