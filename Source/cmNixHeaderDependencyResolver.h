#pragma once

#include <map>
#include <mutex>
#include <string>
#include <vector>

class cmGeneratedFileStream;
class cmGlobalNixGenerator;

class cmNixHeaderDependencyResolver
{
public:
  cmNixHeaderDependencyResolver(cmGlobalNixGenerator* generator);

  // Process header dependencies for a source file
  void ProcessHeaderDependencies(const std::vector<std::string>& headers,
                                const std::string& buildDir,
                                const std::string& srcDir,
                                std::vector<std::string>& existingFiles,
                                std::vector<std::string>& generatedFiles,
                                std::vector<std::string>& configTimeGeneratedFiles);

  // Filter project headers (exclude external headers)
  std::vector<std::string> FilterProjectHeaders(const std::vector<std::string>& headers);

  // Write all external header derivations
  void WriteExternalHeaderDerivations(cmGeneratedFileStream& nixFileStream);

  // Get or create a header derivation for a source directory
  std::string GetOrCreateHeaderDerivation(const std::string& sourceDir, 
                                         const std::vector<std::string>& headers);

  // Clear all cached data
  void Clear();

  // Get header derivation name for a source file
  std::string GetSourceHeaderDerivation(const std::string& sourceFile) const;

  // Set header derivation name for a source file
  void SetSourceHeaderDerivation(const std::string& sourceFile, const std::string& derivationName);

private:
  // Information about a header derivation for a specific source directory
  struct HeaderDerivationInfo {
    std::string Name;
    std::vector<std::string> Headers;
  };

  cmGlobalNixGenerator* Generator;
  
  // Map from source directory to header derivation info
  std::map<std::string, HeaderDerivationInfo> ExternalHeaderDerivations;
  
  // Map from source file to header derivation name (for easy lookup)
  std::map<std::string, std::string> SourceToHeaderDerivation;
  
  // Mutex for thread-safe access
  mutable std::mutex ExternalHeaderMutex;
};