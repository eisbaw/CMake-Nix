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

  void AddObjectDerivation(std::string const& targetName, std::string const& derivationName, std::string const& sourceFile, std::string const& objectFileName, std::string const& language, std::vector<std::string> const& dependencies);

protected:
  virtual void WriteNixFile();
  void WriteDerivations();
  virtual void WritePerTranslationUnitDerivations(cmGeneratedFileStream& nixFileStream);
  virtual void WriteLinkingDerivations(cmGeneratedFileStream& nixFileStream);
  void WriteObjectDerivation(cmGeneratedFileStream& nixFileStream, 
                            cmGeneratorTarget* target, cmSourceFile* source);
  void WriteLinkDerivation(cmGeneratedFileStream& nixFileStream, 
                          cmGeneratorTarget* target);
  
  // Helper methods for WriteLinkDerivation refactoring
  void WriteLinkDerivationHeader(cmGeneratedFileStream& nixFileStream,
                                 cmGeneratorTarget* target,
                                 const std::string& derivName,
                                 const std::string& outputName);
  void WriteLinkDerivationBuildInputs(cmGeneratedFileStream& nixFileStream,
                                      cmGeneratorTarget* target,
                                      const std::string& config);
  void WriteLinkDerivationObjects(cmGeneratedFileStream& nixFileStream,
                                  cmGeneratorTarget* target,
                                  const std::string& config);
protected:
  std::string GetLinkFlags(cmGeneratorTarget* target,
                          const std::string& config);
  void WriteLinkDerivationBuildPhase(cmGeneratedFileStream& nixFileStream,
                                     cmGeneratorTarget* target,
                                     const std::string& linkFlags,
                                     const std::string& primaryLang);

  // Custom command support
  void WriteCustomCommandDerivations(cmGeneratedFileStream& nixFileStream);
  std::map<std::string, std::string> CollectCustomCommands();

  // Install rule support
  void WriteInstallRules(cmGeneratedFileStream& nixFileStream);
  void WriteInstallOutputs(cmGeneratedFileStream& nixFileStream);
  void CollectInstallTargets();

protected:
  std::string GetDerivationName(std::string const& targetName, 
                               std::string const& sourceFile = "") const;

protected:
  // Compiler detection methods
  std::string GetCompilerPackage(const std::string& lang) const;

private:
  std::vector<std::string> GetSourceDependencies(std::string const& sourceFile) const;
  std::string GetCompilerCommand(const std::string& lang) const;
  
  // Configuration handling
  std::string GetBuildConfiguration(cmGeneratorTarget* target) const;
protected:
  // Platform abstraction helpers
  std::string GetObjectFileExtension() const { return ".o"; }
  std::string GetStaticLibraryExtension() const { return ".a"; }
  std::string GetSharedLibraryExtension() const { return ".so"; }
  std::string GetLibraryPrefix() const { return "lib"; }
  std::string GetInstallBinDir() const { return "bin"; }
  std::string GetInstallLibDir() const { return "lib"; }
  std::string GetInstallIncludeDir() const { return "include"; }

private:
  
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
  
  // Structure to store custom command information for dependency resolution
  struct CustomCommandInfo {
    std::string DerivationName;
    std::vector<std::string> Outputs;
    std::vector<std::string> Depends;
    cmCustomCommand const* Command;
    cmLocalGenerator* LocalGen;
  };
  std::vector<CustomCommandInfo> CustomCommands;
  
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

  struct ObjectDerivation {
    std::string TargetName;
    std::string DerivationName;
    std::string SourceFile;
    std::string ObjectFileName;
    std::string Language;
    std::vector<std::string> Dependencies;
  };
  std::map<std::string, ObjectDerivation> ObjectDerivations;
}; 