/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */
#include "cmLocalNixGenerator.h"

#include <cm/memory>

#include "cmGeneratorTarget.h"
#include "cmGlobalNixGenerator.h"
#include "cmMakefile.h"
#include "cmRulePlaceholderExpander.h"
#include "cmSourceFile.h"
#include "cmSystemTools.h"

cmLocalNixGenerator::cmLocalNixGenerator(cmGlobalGenerator* gg, cmMakefile* mf)
  : cmLocalCommonGenerator(gg, mf)
{
}

void cmLocalNixGenerator::Generate()
{
  // Generate target manifest
  this->GenerateTargetManifest();
  
  // Write local targets
  this->WriteLocalTargets();
}

void cmLocalNixGenerator::GenerateTargetManifest()
{
  // Implementation for generating target manifest
  // Called by parent cmGlobalGenerator
}

std::unique_ptr<cmRulePlaceholderExpander>
cmLocalNixGenerator::CreateRulePlaceholderExpander(cmBuildStep buildStep) const
{
  return std::unique_ptr<cmRulePlaceholderExpander>(
    new cmRulePlaceholderExpander(
      this->GetCompilerLauncher(this->GetMakefile(), "C", buildStep),
      this->GetCompilerLauncher(this->GetMakefile(), "CXX", buildStep),
      this->GetCompilerLauncher(this->GetMakefile(), "CUDA", buildStep),
      this->GetCompilerLauncher(this->GetMakefile(), "Fortran", buildStep),
      this->GetCompilerLauncher(this->GetMakefile(), "HIP", buildStep),
      this->GetCompilerLauncher(this->GetMakefile(), "ISPC", buildStep),
      this->GetCompilerLauncher(this->GetMakefile(), "Swift", buildStep)));
}

cmGlobalNixGenerator const* cmLocalNixGenerator::GetGlobalNixGenerator() const
{
  return static_cast<cmGlobalNixGenerator const*>(this->GlobalGenerator);
}

cmGlobalNixGenerator* cmLocalNixGenerator::GetGlobalNixGenerator()
{
  return static_cast<cmGlobalNixGenerator*>(this->GlobalGenerator);
}

void cmLocalNixGenerator::WriteLocalTargets()
{
  auto const& targets = this->GetGeneratorTargets();
  for (auto const& target : targets) {
    if (target->GetType() == cmStateEnums::EXECUTABLE ||
        target->GetType() == cmStateEnums::STATIC_LIBRARY ||
        target->GetType() == cmStateEnums::SHARED_LIBRARY) {
      this->WriteTargetDerivations(target.get());
    }
  }
}

void cmLocalNixGenerator::WriteTargetDerivations(cmGeneratorTarget* target)
{
  // Write compile derivations for each source file
  this->WriteCompileDerivations(target);
  
  // Write linking derivation
  this->WriteLinkDerivation(target);
}

void cmLocalNixGenerator::WriteCompileDerivations(cmGeneratorTarget* target)
{
  // TODO: Implement per-source-file compilation derivations
  // This is Phase 1 core functionality
}

void cmLocalNixGenerator::WriteLinkDerivation(cmGeneratorTarget* target)
{
  // TODO: Implement linking derivation generation
  // This is Phase 1 core functionality
} 