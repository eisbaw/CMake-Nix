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

class cmGeneratedFileStream;
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
  
  // Batch try_compile operations for parallel execution
  void BeginTryCompileBatch();
  void EndTryCompileBatch();

protected:
  void WriteNixFile();
  void WriteDerivations();
  void WritePerTranslationUnitDerivations(cmGeneratedFileStream& nixFileStream);
  void WriteLinkingDerivations(cmGeneratedFileStream& nixFileStream);
  void WriteObjectDerivation(cmGeneratedFileStream& nixFileStream, 
                            cmGeneratorTarget* target, cmSourceFile* source);
  void WriteLinkDerivation(cmGeneratedFileStream& nixFileStream, 
                          cmGeneratorTarget* target);

  // Custom command support
  void WriteCustomCommandDerivations(cmGeneratedFileStream& nixFileStream);
  std::map<std::string, std::string> CollectCustomCommands();

  // Install rule support
  void WriteInstallRules(cmGeneratedFileStream& nixFileStream);
  void WriteInstallOutputs(cmGeneratedFileStream& nixFileStream);
  void CollectInstallTargets();

private:
  std::string GetDerivationName(std::string const& targetName, 
                               std::string const& sourceFile = "") const;
  std::vector<std::string> GetSourceDependencies(std::string const& sourceFile) const;
  
  // Compiler detection methods
  std::string GetCompilerPackage(const std::string& lang) const;
  std::string GetCompilerCommand(const std::string& lang) const;
  
  // Performance optimization: Cache frequently computed values
  mutable std::map<std::string, std::string> CompilerPackageCache;
  mutable std::map<std::string, std::string> CompilerCommandCache;
  mutable std::map<std::pair<cmGeneratorTarget*, std::string>, std::vector<std::string>> LibraryDependencyCache;
  mutable std::map<std::string, std::string> DerivationNameCache;
  
  // Install rule tracking
  std::vector<cmGeneratorTarget*> InstallTargets;
  
  // Common string constants to avoid repeated allocations
  static const std::string DefaultConfig;
  static const std::string CLanguage;
  static const std::string CXXLanguage;
  static const std::string GccCompiler;
  static const std::string ClangCompiler;
  
  // Map from output file to custom command derivation name
  std::map<std::string, std::string> CustomCommandOutputs;
  
  // Dependency graph infrastructure for transitive dependency resolution
  class cmNixDependencyNode {
  public:
    std::string targetName;
    cmStateEnums::TargetType type;
    std::vector<std::string> directDependencies;
    mutable std::set<std::string> transitiveDependencies; // cached
    mutable bool transitiveDepsComputed = false;
  };
  
  class cmNixDependencyGraph {
    std::map<std::string, cmNixDependencyNode> nodes;
    
  public:
    void AddTarget(const std::string& name, cmGeneratorTarget* target);
    void AddDependency(const std::string& from, const std::string& to);
    std::set<std::string> GetTransitiveSharedLibraries(const std::string& target) const;
    bool HasCircularDependency() const;
    void Clear();
  };
  
  // Dependency graph instance
  mutable cmNixDependencyGraph DependencyGraph;
  
  // Build dependency graph from all targets
  void BuildDependencyGraph();
}; 