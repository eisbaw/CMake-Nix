/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */
#pragma once

#include "cmConfigure.h" // IWYU pragma: keep

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "cmCommonTargetGenerator.h"
#include "cmNixPackageMapper.h"

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

  /// Get header dependencies for a source file
  std::vector<std::string> GetSourceDependencies(cmSourceFile const* source) const;

  /// Pure Nix library support - public for global generator access
  std::vector<std::string> GetTargetLibraryDependencies(std::string const& config) const;
  std::string FindOrCreateNixPackage(std::string const& libName) const;
  const cmNixPackageMapper& GetPackageMapper() const { return PackageMapper; }

  // Pure virtual methods from cmCommonTargetGenerator
  void AddIncludeFlags(std::string& flags, std::string const& lang,
                       std::string const& config) override;
  std::string GetClangTidyReplacementsFilePath(
    std::string const& directory, cmSourceFile const& source,
    std::string const& config) const override;

public:
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

  /// Option B: Compiler-based dependency scanning methods
  std::vector<std::string> ScanWithCompiler(cmSourceFile const* source, std::string const& lang) const;
  std::vector<std::string> GetManualDependencies(cmSourceFile const* source) const;
  std::vector<std::string> ScanWithRegex(cmSourceFile const* source, std::string const& lang) const;
  
  /// Helper methods for dependency scanning
  std::string GetCompilerCommand(std::string const& lang) const;
  std::vector<std::string> GetCompileFlags(std::string const& lang, std::string const& config) const;
  std::vector<std::string> GetIncludeFlags(std::string const& lang, std::string const& config) const;
  std::vector<std::string> ParseCompilerDependencyOutput(std::string const& output, cmSourceFile const* source) const;
  std::string ResolveIncludePath(std::string const& headerName) const;

  /// Pure Nix library support methods (private implementation)
  bool CreateNixPackageFile(std::string const& libName, std::string const& filePath) const;
  std::string MapCommonLibraryToNixPackage(std::string const& libName) const;
  std::string GetNixPackageFilePath(std::string const& libName) const;

private:
  cmLocalNixGenerator* LocalGenerator;
  cmMakefile* Makefile;
  cmNixPackageMapper PackageMapper;
}; 