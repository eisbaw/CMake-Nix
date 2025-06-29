/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */
#include "cmNixTargetGenerator.h"

#include <cm/memory>
#include <sstream>

#include "cmGeneratorTarget.h"
#include "cmGlobalNixGenerator.h"
#include "cmLocalNixGenerator.h"
#include "cmMakefile.h"
#include "cmSourceFile.h"
#include "cmSystemTools.h"

std::unique_ptr<cmNixTargetGenerator> cmNixTargetGenerator::New(
  cmGeneratorTarget* target)
{
  return std::unique_ptr<cmNixTargetGenerator>(
    new cmNixTargetGenerator(target));
}

cmNixTargetGenerator::cmNixTargetGenerator(cmGeneratorTarget* target)
  : cmCommonTargetGenerator(target)
  , LocalGenerator(
      static_cast<cmLocalNixGenerator*>(target->GetLocalGenerator()))
  , Makefile(target->Target->GetMakefile())
{
}

cmNixTargetGenerator::~cmNixTargetGenerator() = default;

void cmNixTargetGenerator::Generate()
{
  // Generate per-source file derivations
  this->WriteObjectDerivations();
  
  // Generate linking derivation
  this->WriteLinkDerivation();
}

std::string cmNixTargetGenerator::GetTargetName() const
{
  return this->GeneratorTarget->GetName();
}

void cmNixTargetGenerator::WriteObjectDerivations()
{
  // Get all source files for this target
  std::vector<cmSourceFile*> sources;
  this->GeneratorTarget->GetSourceFiles(sources, "");
  
  for (cmSourceFile* source : sources) {
    // Only process C/C++ source files for Phase 1
    std::string const& lang = source->GetLanguage();
    if (lang == "C" || lang == "CXX") {
      // TODO: Write derivation for this source file
      // This is Phase 1 core functionality
    }
  }
}

void cmNixTargetGenerator::WriteLinkDerivation()
{
  // TODO: Generate linking derivation for this target  
  // This is Phase 1 core functionality
}

std::string cmNixTargetGenerator::GetDerivationName(
  cmSourceFile const* source) const
{
  std::string sourcePath = source->GetFullPath();
  
  // Convert to relative path from source directory
  std::string relPath = cmSystemTools::RelativePath(
    this->GetMakefile()->GetCurrentSourceDirectory(), sourcePath);
  
  // Convert path to valid Nix identifier
  std::string name = relPath;
  std::replace(name.begin(), name.end(), '/', '_');
  std::replace(name.begin(), name.end(), '.', '_');
  
  return this->GetTargetName() + "_" + name + "_o";
}

std::string cmNixTargetGenerator::GetObjectFileName(
  cmSourceFile const* source) const
{
  std::string sourcePath = source->GetFullPath();
  std::string objectName = cmSystemTools::GetFilenameWithoutLastExtension(
    cmSystemTools::GetFilenameName(sourcePath));
  return objectName + ".o";
}

std::vector<std::string> cmNixTargetGenerator::GetSourceDependencies(
  cmSourceFile const* source) const
{
  // TODO: Implement header dependency tracking using CMake's dependency analysis
  // This is Phase 1 core functionality
  return {};
} 