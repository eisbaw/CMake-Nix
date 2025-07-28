/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */
#pragma once

#include "cmConfigure.h" // IWYU pragma: keep

#include <chrono>
#include <iosfwd>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "cmGlobalCommonGenerator.h"
#include "cmGlobalGeneratorFactory.h"

class cmGeneratedFileStream;
class cmGeneratorTarget;
class cmLocalGenerator;
class cmMakefile;
class cmake;
class cmNixWriter;
class cmNixCompilerResolver;
class cmNixDerivationWriter;
class cmNixDependencyGraph;
class cmNixCustomCommandHandler;
class cmNixHeaderDependencyResolver;
class cmNixCacheManager;
class cmNixFileSystemHelper;

/**
 * \class cmGlobalNixGenerator
 * \brief Write Nix expressions for building C/C++ projects.
 *
 * Error Handling Policy:
 * - FATAL_ERROR (IssueMessage): Use for configuration errors that prevent generation
 *   (e.g., missing required files, invalid configuration)
 * - WARNING (IssueMessage): Use for recoverable issues that users should know about
 *   (e.g., unsupported features, external file warnings, fallback behaviors)
 * - Debug output (std::cerr with [NIX-DEBUG]): Use only when GetDebugOutput() is true
 *   for diagnostic information during development
 * - Never use raw std::cerr for user-facing messages
 *
 * The Nix generator produces fine-grained Nix derivations that maximize
 * build parallelism and caching efficiency. It generates one derivation
 * per translation unit and separate derivations for linking.
 */
class cmGlobalNixGenerator : public cmGlobalCommonGenerator
{
public:
  explicit cmGlobalNixGenerator(cmake* cm);
  ~cmGlobalNixGenerator();
  
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
  void WriteNixHelperFunctions(cmNixWriter& writer);
  void WriteDerivations();
  virtual void WritePerTranslationUnitDerivations(cmGeneratedFileStream& nixFileStream);
  virtual void WriteLinkingDerivations(cmGeneratedFileStream& nixFileStream);
  void WriteObjectDerivation(cmGeneratedFileStream& nixFileStream, 
                            cmGeneratorTarget* target, const cmSourceFile* source);
  void WriteLinkDerivation(cmGeneratedFileStream& nixFileStream, 
                          cmGeneratorTarget* target);
  
  // New helper methods for object derivation refactoring
  void WriteObjectDerivationUsingWriter(cmNixWriter& writer, 
                                       cmGeneratorTarget* target, 
                                       const cmSourceFile* source);
  
  // Helper methods for WriteObjectDerivation decomposition
  bool ValidateSourceFile(const cmSourceFile* source, 
                         cmGeneratorTarget* target,
                         std::string& errorMessage);
  void WriteExternalSourceDerivation(cmGeneratedFileStream& nixFileStream,
                                    cmGeneratorTarget* target,
                                    const cmSourceFile* source,
                                    const std::string& lang,
                                    const std::string& derivName,
                                    const std::string& objectName);
  void WriteRegularSourceDerivation(cmGeneratedFileStream& nixFileStream,
                                   cmGeneratorTarget* target,
                                   const cmSourceFile* source,
                                   const std::string& lang,
                                   const std::string& derivName,
                                   const std::string& objectName);
  std::string DetermineCompilerPackage(cmGeneratorTarget* target,
                                      const cmSourceFile* source) const;
  std::string GetCompileFlags(cmGeneratorTarget* target,
                             const cmSourceFile* source,
                             const std::string& lang,
                             const std::string& config,
                             const std::string& objectName);
  
  // Additional helper methods for WriteObjectDerivation decomposition
  void WriteCompositeSource(cmGeneratedFileStream& nixFileStream,
                           const std::vector<std::string>& configTimeGeneratedFiles,
                           const std::string& srcDir,
                           const std::string& buildDir,
                           cmGeneratorTarget* target = nullptr,
                           const std::string& lang = "",
                           const std::string& config = "",
                           const std::vector<std::string>& customCommandHeaders = {});
  void WriteFilesetUnion(cmGeneratedFileStream& nixFileStream,
                        const std::vector<std::string>& existingFiles,
                        const std::vector<std::string>& generatedFiles,
                        const std::string& rootPath);
  std::vector<std::string> BuildBuildInputsList(cmGeneratorTarget* target,
                                               const cmSourceFile* source,
                                               const std::string& config,
                                               const std::string& sourceFile,
                                               const std::string& projectSourceRelPath);
  
  // System path detection helper
  bool IsSystemPath(const std::string& path) const;
  
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

  // Custom command support - delegated to cmNixCustomCommandHandler
  void WriteCustomCommandDerivations(cmGeneratedFileStream& nixFileStream);
  

  // Install rule support
  void WriteInstallRules(cmGeneratedFileStream& nixFileStream);
  void WriteInstallOutputs(cmGeneratedFileStream& nixFileStream);
  void CollectInstallTargets();
  
  // Check for incompatible features
  void CheckForExternalProjectUsage();
  void GenerateSkeletonPackageFiles();

protected:
  std::string GetDerivationName(std::string const& targetName, 
                               std::string const& sourceFile = "") const;

protected:
  // Compiler detection methods
  std::string GetCompilerPackage(const std::string& lang) const;

protected:
  bool UseExplicitSources() const;
  void WriteExplicitSourceDerivation(cmGeneratedFileStream& nixFileStream,
                                    const std::string& sourceFile,
                                    const std::vector<std::string>& dependencies,
                                    const std::string& projectSourceRelPath);

private:
  std::vector<std::string> GetSourceDependencies(std::string const& sourceFile) const;
  std::string GetCompilerCommand(const std::string& lang) const;
  
  // Configuration handling
  std::string GetBuildConfiguration(cmGeneratorTarget* target) const;
  
  // Helper method to get library dependencies with caching
  std::vector<std::string> GetCachedLibraryDependencies(cmGeneratorTarget* target, 
                                                        const std::string& config) const;
  
  // Helper method to process library dependencies for buildInputs
  void ProcessLibraryDependenciesForBuildInputs(
    const std::vector<std::string>& libraryDeps,
    std::vector<std::string>& buildInputs,
    const std::string& projectSourceRelPath) const;
  
  // Helper method to process library dependencies for linking
  void ProcessLibraryDependenciesForLinking(
    cmGeneratorTarget* target,
    const std::string& config,
    std::vector<std::string>& linkFlagsList,
    std::vector<std::string>& libraries,
    std::set<std::string>& transitiveDeps) const;
  
  // Debug output helper - centralizes debug logging
  void LogDebug(const std::string& message) const;
  
protected:
  // Platform abstraction helpers
  // Note: These methods return Unix-specific conventions since Nix only supports Unix/Linux platforms
  // Windows DLLs and macOS dylibs are not supported by the Nix package manager
  std::string GetObjectFileExtension() const { return ".o"; }
  std::string GetStaticLibraryExtension() const { return ".a"; }
  std::string GetSharedLibraryExtension() const { return ".so"; }
  std::string GetLibraryPrefix() const { return "lib"; }
  std::string GetInstallBinDir() const { return "bin"; }
  std::string GetInstallLibDir() const { return "lib"; }
  std::string GetInstallIncludeDir() const { return "include"; }

private:
  // Profiling support
  bool GetProfilingEnabled() const;
  
  // Simple profiling timer class
  class ProfileTimer {
  public:
    ProfileTimer(const cmGlobalNixGenerator* gen, const std::string& name);
    ~ProfileTimer();
  private:
    const cmGlobalNixGenerator* Generator;
    std::string Name;
    std::chrono::steady_clock::time_point StartTime;
  };
  
  // Compiler resolution utility
  mutable std::unique_ptr<cmNixCompilerResolver> CompilerResolver;
  
  // Derivation writer utility
  mutable std::unique_ptr<cmNixDerivationWriter> DerivationWriter;
  
  // Cache manager for performance optimization
  mutable std::unique_ptr<cmNixCacheManager> CacheManager;
  
  // File system helper for path operations
  mutable std::unique_ptr<cmNixFileSystemHelper> FileSystemHelper;
  
  // Install rule tracking
  std::vector<cmGeneratorTarget*> InstallTargets;
  mutable std::mutex InstallTargetsMutex;
  
  // Common string constants to avoid repeated allocations
  static const std::string DefaultConfig;
  static const std::string CLanguage;
  static const std::string CXXLanguage;
  
  // Numeric constants
  static constexpr int MAX_CYCLE_DETECTION_DEPTH = 100; // Maximum depth for cycle detection
  
  /**
   * Maximum number of external headers to include per source file.
   * When building projects with external source files (outside the project tree),
   * we need to copy their header dependencies. This limit prevents excessive
   * copying for projects like Zephyr RTOS that may have thousands of headers.
   * 
   * A limit of 100 headers per source file is sufficient for most use cases
   * while preventing build timeouts. Projects needing more can set
   * CMAKE_NIX_EXTERNAL_HEADER_LIMIT to override this value.
   * 
   * Note: With unified header derivations, this limit is less critical as
   * headers are shared across sources from the same directory.
   */
  static constexpr size_t MAX_EXTERNAL_HEADERS_PER_SOURCE = 100;
  
  // Track used derivation names to ensure uniqueness
  mutable std::set<std::string> UsedDerivationNames;
  mutable std::mutex UsedNamesMutex;
  
  // Map from output file to custom command derivation name
  std::map<std::string, std::string> CustomCommandOutputs;
  
  // Map from object file path to compilation derivation name
  std::map<std::string, std::string> ObjectFileOutputs;
  
  // Custom command handling is delegated to cmNixCustomCommandHandler
  std::unique_ptr<cmNixCustomCommandHandler> CustomCommandHandler;
  
  // Install rule handling is delegated to cmNixInstallRuleGenerator
  std::unique_ptr<class cmNixInstallRuleGenerator> InstallRuleGenerator;
  
  // Header dependency resolver
  mutable std::unique_ptr<cmNixHeaderDependencyResolver> HeaderDependencyResolver;
  
  
  // Dependency graph instance
  mutable std::unique_ptr<cmNixDependencyGraph> DependencyGraph;
  
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