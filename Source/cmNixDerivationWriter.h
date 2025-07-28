/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */
#pragma once

#include "cmConfigure.h" // IWYU pragma: keep

#include <string>
#include <vector>

class cmGeneratedFileStream;
class cmGeneratorTarget;
class cmSourceFile;
class cmNixWriter;

/**
 * \class cmNixDerivationWriter
 * \brief Encapsulates Nix derivation writing logic for the CMake Nix backend.
 *
 * This class handles the generation of Nix derivations for object files,
 * linking operations, and custom commands. It extracts and centralizes the
 * derivation writing functionality that was previously embedded in
 * cmGlobalNixGenerator.
 */
class cmNixDerivationWriter
{
public:
  cmNixDerivationWriter();
  ~cmNixDerivationWriter();

  /**
   * Write an object file derivation for a single source file using cmakeNixCC helper.
   * Creates a fine-grained Nix derivation that compiles one translation unit.
   */
  void WriteObjectDerivationWithHelper(cmGeneratedFileStream& nixFileStream,
                                      const std::string& derivName,
                                      const std::string& objectName,
                                      const std::string& srcPath,
                                      const std::string& sourcePath,
                                      const std::string& compilerPackage,
                                      const std::string& compileFlags,
                                      const std::vector<std::string>& buildInputs);

  /**
   * Write a link derivation for an executable or library target using cmakeNixLD helper.
   * Creates a Nix derivation that links object files into the final output.
   */
  void WriteLinkDerivationWithHelper(cmGeneratedFileStream& nixFileStream,
                                    const std::string& derivName,
                                    const std::string& targetName,
                                    const std::string& targetType,
                                    const std::vector<std::string>& buildInputs,
                                    const std::vector<std::string>& objects,
                                    const std::string& compilerPackage,
                                    const std::string& compilerCommand,
                                    const std::string& flags,
                                    const std::vector<std::string>& libraries,
                                    const std::string& version,
                                    const std::string& soversion,
                                    const std::string& postBuildPhase = "");

  /**
   * Write a custom command derivation.
   * Creates a Nix derivation that executes arbitrary commands.
   */
  void WriteCustomCommandDerivation(cmGeneratedFileStream& nixFileStream,
                                   const std::string& derivName,
                                   const std::vector<std::string>& outputs,
                                   const std::vector<std::string>& depends,
                                   const std::vector<std::string>& commands,
                                   const std::string& workingDir,
                                   const std::string& srcPath);

  /**
   * Write common Nix helper functions used by derivations.
   * Includes utilities for fileset unions, source composition, etc.
   */
  void WriteNixHelperFunctions(cmNixWriter& writer);

  /**
   * Write a fileset union expression for including multiple files.
   */
  void WriteFilesetUnion(cmGeneratedFileStream& nixFileStream,
                        const std::vector<std::string>& existingFiles,
                        const std::vector<std::string>& generatedFiles,
                        const std::string& rootPath);

  /**
   * Write composite source derivation for configuration-time generated files.
   */
  void WriteCompositeSource(cmGeneratedFileStream& nixFileStream,
                           const std::vector<std::string>& configTimeGeneratedFiles,
                           const std::string& srcDir,
                           const std::string& buildDir,
                           [[maybe_unused]] cmGeneratorTarget* target = nullptr,
                           const std::string& lang = "",
                           const std::string& config = "",
                           const std::vector<std::string>& customCommandHeaders = {});

  /**
   * Write an external header derivation for sharing headers across sources.
   */
  void WriteExternalHeaderDerivation(cmGeneratedFileStream& nixFileStream,
                                    const std::string& derivName,
                                    const std::vector<std::string>& headers,
                                    const std::string& sourceDir);

  /**
   * Write an install rule derivation.
   */
  void WriteInstallDerivation(cmGeneratedFileStream& nixFileStream,
                             const std::string& derivName,
                             const std::string& sourcePath,
                             const std::string& installCommand);

  /**
   * Set whether to enable debug output.
   */
  void SetDebugOutput(bool debug) { this->DebugOutput = debug; }

  /**
   * Set the object file extension (platform-specific).
   */
  void SetObjectFileExtension(const std::string& ext) { this->ObjectFileExtension = ext; }

  /**
   * Set the shared library extension (platform-specific).
   */
  void SetSharedLibraryExtension(const std::string& ext) { this->SharedLibraryExtension = ext; }

  /**
   * Set the static library extension (platform-specific).
   */
  void SetStaticLibraryExtension(const std::string& ext) { this->StaticLibraryExtension = ext; }

  /**
   * Set the library prefix (platform-specific).
   */
  void SetLibraryPrefix(const std::string& prefix) { this->LibraryPrefix = prefix; }

private:
  // Debug output helper
  void LogDebug(const std::string& message) const;

  // Platform-specific settings
  std::string ObjectFileExtension = ".o";
  std::string SharedLibraryExtension = ".so";
  std::string StaticLibraryExtension = ".a";
  std::string LibraryPrefix = "lib";

  // Debug flag
  bool DebugOutput = false;
};