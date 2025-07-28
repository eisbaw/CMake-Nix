#include "cmNixHeaderDependencyResolver.h"

#include "cmGeneratedFileStream.h"
#include "cmGlobalNixGenerator.h"
#include "cmNixDerivationWriter.h"
#include "cmNixPathUtils.h"
#include "cmSystemTools.h"
#include "cmake.h"

#include <algorithm>
#include <iostream>

cmNixHeaderDependencyResolver::cmNixHeaderDependencyResolver(cmGlobalNixGenerator* generator)
  : Generator(generator)
{
}

void cmNixHeaderDependencyResolver::ProcessHeaderDependencies(
  const std::vector<std::string>& headers,
  const std::string& buildDir,
  const std::string& srcDir,
  std::vector<std::string>& existingFiles,
  std::vector<std::string>& generatedFiles,
  std::vector<std::string>& configTimeGeneratedFiles)
{
  for (const std::string& header : headers) {
    // Normalize path for consistency
    std::string normalizedHeader = header;
    cmSystemTools::ConvertToUnixSlashes(normalizedHeader);
    
    // Check if the header is generated at build time or configure time
    if (header.find(buildDir) == 0) {
      // Header is in the build directory
      std::string relativePath = header.substr(buildDir.length());
      if (relativePath[0] == '/') {
        relativePath = relativePath.substr(1);
      }
      
      // Check if it's a configure-time generated file
      std::string fullPath = buildDir + "/" + relativePath;
      if (cmSystemTools::FileExists(fullPath)) {
        configTimeGeneratedFiles.push_back(normalizedHeader);
      } else {
        generatedFiles.push_back(normalizedHeader);
      }
    } else if (header.find(srcDir) == 0 || 
               cmSystemTools::FileIsFullPath(header)) {
      // Check if file exists
      if (cmSystemTools::FileExists(header)) {
        existingFiles.push_back(normalizedHeader);
      } else {
        // It might be a generated file with absolute path
        generatedFiles.push_back(normalizedHeader);
      }
    } else {
      // Relative path - resolve relative to source directory
      std::string fullPath = srcDir + "/" + header;
      if (cmSystemTools::FileExists(fullPath)) {
        existingFiles.push_back(normalizedHeader);
      } else {
        // Check if it exists in build directory
        fullPath = buildDir + "/" + header;
        if (cmSystemTools::FileExists(fullPath)) {
          configTimeGeneratedFiles.push_back(normalizedHeader);
        } else {
          generatedFiles.push_back(normalizedHeader);
        }
      }
    }
  }
}

std::vector<std::string> cmNixHeaderDependencyResolver::FilterProjectHeaders(
  const std::vector<std::string>& headers)
{
  std::vector<std::string> projectHeaders;
  std::string projectDir = this->Generator->GetCMakeInstance()->GetHomeDirectory();
  
  for (const std::string& header : headers) {
    // Skip external headers (system paths, Nix store paths, etc.)
    if (cmNixPathUtils::IsExternalPath(header, projectDir)) {
      continue;
    }
    
    projectHeaders.push_back(header);
  }
  
  return projectHeaders;
}

void cmNixHeaderDependencyResolver::WriteExternalHeaderDerivations(
  cmGeneratedFileStream& nixFileStream)
{
  std::lock_guard<std::mutex> lock(this->ExternalHeaderMutex);
  
  if (this->ExternalHeaderDerivations.empty()) {
    return;
  }
  
  nixFileStream << "  # External header derivations\n";
  
  for (const auto& pair : this->ExternalHeaderDerivations) {
    const std::string& sourceDir = pair.first;
    const HeaderDerivationInfo& info = pair.second;
    
    if (this->Generator->GetCMakeInstance()->GetDebugOutput()) {
      std::cerr << "[NIX-DEBUG] Writing header derivation " << info.Name 
                << " for " << sourceDir << " with " << info.Headers.size() 
                << " headers" << std::endl;
    }
    
    nixFileStream << "  " << info.Name << " = stdenv.mkDerivation {\n";
    nixFileStream << "    name = \"" << info.Name << "\";\n";
    
    // Source is /. to avoid unnecessary copying  
    nixFileStream << "    src = /.;\n";
    nixFileStream << "    phases = [ \"unpackPhase\" \"installPhase\" ];\n";
    nixFileStream << "    installPhase = ''\n";
    nixFileStream << "      mkdir -p $out\n";
    
    // Copy each header file maintaining directory structure
    for (const std::string& header : info.Headers) {
      std::string destDir = "$out/" + cmSystemTools::GetFilenamePath(header);
      nixFileStream << "      mkdir -p \"" << destDir << "\"\n";
      nixFileStream << "      cp \"" << header << "\" \"" << destDir << "/\"\n";
    }
    
    nixFileStream << "    '';\n";
    
    // Disable fixup phase for headers
    nixFileStream << "    dontFixup = true;\n";
    nixFileStream << "  };\n\n";
  }
}

std::string cmNixHeaderDependencyResolver::GetOrCreateHeaderDerivation(
  const std::string& sourceDir, 
  const std::vector<std::string>& headers)
{
  std::lock_guard<std::mutex> lock(this->ExternalHeaderMutex);
  
  // Check if we already have a derivation for this source directory
  auto it = this->ExternalHeaderDerivations.find(sourceDir);
  if (it != this->ExternalHeaderDerivations.end()) {
    // Update the headers list with any new headers
    for (const std::string& header : headers) {
      if (std::find(it->second.Headers.begin(), it->second.Headers.end(), header) == 
          it->second.Headers.end()) {
        it->second.Headers.push_back(header);
      }
    }
    return it->second.Name;
  }
  
  // Create a new header derivation
  HeaderDerivationInfo info;
  
  // Generate a unique name based on the source directory
  std::string safeName = sourceDir;
  cmSystemTools::ReplaceString(safeName, "/", "_");
  cmSystemTools::ReplaceString(safeName, ".", "_");
  cmSystemTools::ReplaceString(safeName, "-", "_");
  
  // Remove leading underscores
  while (!safeName.empty() && safeName[0] == '_') {
    safeName = safeName.substr(1);
  }
  
  // Ensure the name is not empty and is valid
  if (safeName.empty()) {
    safeName = "headers";
  }
  
  // Make the name unique by appending a counter if needed
  std::string baseName = "headers_" + safeName;
  info.Name = baseName;
  int counter = 1;
  while (std::any_of(this->ExternalHeaderDerivations.begin(), 
                     this->ExternalHeaderDerivations.end(),
                     [&info](const auto& pair) { 
                       return pair.second.Name == info.Name; 
                     })) {
    info.Name = baseName + "_" + std::to_string(counter++);
  }
  
  info.Headers = headers;
  this->ExternalHeaderDerivations[sourceDir] = info;
  
  return info.Name;
}

void cmNixHeaderDependencyResolver::Clear()
{
  std::lock_guard<std::mutex> lock(this->ExternalHeaderMutex);
  this->ExternalHeaderDerivations.clear();
  this->SourceToHeaderDerivation.clear();
}

std::string cmNixHeaderDependencyResolver::GetSourceHeaderDerivation(const std::string& sourceFile) const
{
  std::lock_guard<std::mutex> lock(this->ExternalHeaderMutex);
  auto it = this->SourceToHeaderDerivation.find(sourceFile);
  if (it != this->SourceToHeaderDerivation.end()) {
    return it->second;
  }
  return "";
}

void cmNixHeaderDependencyResolver::SetSourceHeaderDerivation(const std::string& sourceFile, const std::string& derivationName)
{
  std::lock_guard<std::mutex> lock(this->ExternalHeaderMutex);
  this->SourceToHeaderDerivation[sourceFile] = derivationName;
}