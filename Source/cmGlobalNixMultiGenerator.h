/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */
#pragma once

#include "cmGlobalNixGenerator.h"

#include <string>
#include <vector>
#include <memory>

/** \class cmGlobalNixMultiGenerator
 * \brief Nix generator supporting multiple configurations
 *
 * cmGlobalNixMultiGenerator generates a Nix expression that builds multiple
 * configurations (Debug, Release, etc.) from a single CMake generation.
 */
class cmGlobalNixMultiGenerator : public cmGlobalNixGenerator
{
public:
  cmGlobalNixMultiGenerator(cmake* cm);

  static std::unique_ptr<cmGlobalGeneratorFactory> NewFactory();

  /** Get the name of this generator */
  std::string GetName() const override { return "Nix Multi-Config"; }
  
  /** Get the actual name of this generator */
  static std::string GetActualName() { return "Nix Multi-Config"; }
  
  /** Return true if this is a multi-config generator */
  bool IsMultiConfig() const override { return true; }
  
  /** Get the list of configurations */
  std::vector<std::string> GetConfigurationTypes() const;
  
  /** Get the default configuration */
  std::string GetDefaultConfiguration() const;

protected:
  /** Generate the Nix expression file */
  void WriteNixFile() override;
  
  /** Write per-translation-unit derivations for all configurations */
  void WritePerTranslationUnitDerivations(cmGeneratedFileStream& nixFileStream) override;
  
  /** Write linking derivations for all configurations */  
  void WriteLinkingDerivations(cmGeneratedFileStream& nixFileStream) override;
  
  /** Write an object derivation for a specific configuration */
  void WriteObjectDerivationForConfig(cmGeneratedFileStream& nixFileStream, 
                                      cmGeneratorTarget* target, 
                                      cmSourceFile* source,
                                      const std::string& config);
  
  /** Write a link derivation for a specific configuration */
  void WriteLinkDerivationForConfig(cmGeneratedFileStream& nixFileStream,
                                    cmGeneratorTarget* target,
                                    const std::string& config);
                                    
  /** Get derivation name with configuration suffix */
  std::string GetDerivationNameForConfig(const std::string& targetName,
                                         const std::string& sourceFile,
                                         const std::string& config) const;
};