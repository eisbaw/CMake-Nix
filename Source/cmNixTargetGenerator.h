/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */
#pragma once

#include "cmConfigure.h" // IWYU pragma: keep

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "cmCommonTargetGenerator.h"

class cmGeneratorTarget;
class cmLocalNixGenerator;
class cmMakefile;
class cmSourceFile;

class cmNixTargetGenerator : public cmCommonTargetGenerator
{
public:
  /// Create a cmNixTargetGenerator according to the @a target's type.
  static std::unique_ptr<cmNixTargetGenerator> New(cmGeneratorTarget* target);

  /// Build a NixTargetGenerator.
  cmNixTargetGenerator(cmGeneratorTarget* target);

  /// Destructor.
  ~cmNixTargetGenerator() override;

  virtual void Generate();

  std::string GetTargetName() const;

protected:
  cmGeneratorTarget* GetGeneratorTarget() const
  {
    return this->GeneratorTarget;
  }

  cmLocalNixGenerator* GetLocalGenerator() const
  {
    return this->LocalGenerator;
  }

  cmMakefile* GetMakefile() const { return this->Makefile; }

  /// Generate per-source file derivations
  void WriteObjectDerivations();
  
  /// Generate linking derivation
  void WriteLinkDerivation();

  /// Get the derivation name for a source file
  std::string GetDerivationName(cmSourceFile const* source) const;

  /// Get the object file name for a source file
  std::string GetObjectFileName(cmSourceFile const* source) const;

  /// Get header dependencies for a source file
  std::vector<std::string> GetSourceDependencies(cmSourceFile const* source) const;

private:
  cmLocalNixGenerator* LocalGenerator;
  cmMakefile* Makefile;
}; 