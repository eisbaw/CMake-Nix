/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */
#pragma once

#include "cmConfigure.h" // IWYU pragma: keep

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <set>
#include <mutex>

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
  explicit cmNixTargetGenerator(cmGeneratorTarget* target);

  /// Destructor.
  ~cmNixTargetGenerator() override;

  virtual void Generate();

  std::string const& GetTargetName() const;

  /// Get header dependencies for a source file
  std::vector<std::string> GetSourceDependencies(cmSourceFile const* source) const;
  
  /// Get transitive header dependencies for a file
  std::vector<std::string> GetTransitiveDependencies(std::string const& filePath, 
                                                     std::set<std::string>& visited,
                                                     int depth = 0) const;

  /// Pure Nix library support - public for global generator access
  std::vector<std::string> GetTargetLibraryDependencies(std::string const& config) const;
  std::string FindOrCreateNixPackage(std::string const& libName) const;
  const cmNixPackageMapper& GetPackageMapper() const { return cmNixPackageMapper::GetInstance(); }

  // Pure virtual methods from cmCommonTargetGenerator
  void AddIncludeFlags(std::string& flags, std::string const& lang,
                       std::string const& config) override;
  std::string GetClangTidyReplacementsFilePath(
    std::string const& directory, cmSourceFile const& source,
    std::string const& config) const override;

  // Debug output helper - centralizes debug logging
  void LogDebug(const std::string& message) const;

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

public:
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

  /// Precompiled Header (PCH) support
  void WritePchDerivations();
  std::string GetPchDerivationName(std::string const& language, std::string const& arch) const;
  bool NeedsPchSupport(std::string const& config, std::string const& language) const;
  std::vector<std::string> GetPchDependencies(cmSourceFile const* source, std::string const& config) const;

private:
  // Helper method to check if a language is compilable
  bool IsCompilableLanguage(const std::string& lang) const;
  
  // Constants
  static constexpr int MAX_HEADER_RECURSION_DEPTH = 100; // Maximum depth for header dependency scanning
  static constexpr size_t MAX_DEPENDENCY_CACHE_SIZE = 10000; // Maximum entries in dependency cache
  
  cmLocalNixGenerator* LocalGenerator;
  
  // Cache for transitive dependencies to avoid exponential complexity
  // Using mutable to allow caching in const methods
  mutable std::map<std::string, std::vector<std::string>> TransitiveDependencyCache;
  mutable std::mutex TransitiveDependencyCacheMutex;
}; 