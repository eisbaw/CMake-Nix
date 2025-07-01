/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */
#include "cmNixTargetGenerator.h"

#include <cm/memory>
#include <sstream>
#include <algorithm>
#include <set>

#include "cmGeneratorTarget.h"
#include "cmGlobalNixGenerator.h"
#include "cmLocalNixGenerator.h"
#include "cmMakefile.h"
#include "cmSourceFile.h"
#include "cmSystemTools.h"
#include "cmDependsC.h"
#include "cmDepends.h"
#include "cmLocalUnixMakefileGenerator3.h"
#include "cmProcessOutput.h"
#include "cmList.h"
#include "cmListFileCache.h"
#include "cmValue.h"
#include <regex>
#include <fstream>

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
  // Get build configuration
  std::string config = this->GetMakefile()->GetSafeDefinition("CMAKE_BUILD_TYPE");
  if (config.empty()) {
    config = "Release";
  }
  
  // Get library dependencies using pure Nix approach
  std::vector<std::string> libraryDeps = this->GetTargetLibraryDependencies(config);
  
  // Get object file dependencies (from per-translation-unit derivations)
  std::vector<cmSourceFile*> sources;
  this->GeneratorTarget->GetSourceFiles(sources, config);
  
  std::vector<std::string> objectDeps;
  for (cmSourceFile* source : sources) {
    std::string const& lang = source->GetLanguage();
    if (lang == "C" || lang == "CXX") {
      objectDeps.push_back(this->GetDerivationName(source));
    }
  }
  
  // TODO: Write the actual Nix derivation to the global output
  // This will be integrated with cmGlobalNixGenerator's output system
  // For now, we've collected all the dependency information needed
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
  std::vector<std::string> dependencies;
  
  // Get the source language
  std::string const& lang = source->GetLanguage();
  if (lang != "C" && lang != "CXX" && lang != "OBJC" && lang != "OBJCXX" && 
      lang != "CUDA" && lang != "HIP" && lang != "ISPC") {
    return dependencies; // No dependency scanning for this language
  }
  
  // Option B: Compiler-based dependency scanning
  try {
    // Try compiler-based scanning first (most accurate)
    dependencies = this->ScanWithCompiler(source, lang);
    if (!dependencies.empty()) {
      return dependencies;
    }
  } catch (...) {
    // Fall back to other methods if compiler scanning fails
  }
  
  // Fallback 1: Check for manually specified dependencies
  dependencies = this->GetManualDependencies(source);
  if (!dependencies.empty()) {
    return dependencies;
  }
  
  // Fallback 2: Use CMake's regex-based scanner
  dependencies = this->ScanWithRegex(source, lang);
  
  return dependencies;
}

std::vector<std::string> cmNixTargetGenerator::ScanWithCompiler(
  cmSourceFile const* source, std::string const& lang) const
{
  std::vector<std::string> dependencies;
  
  // Get compiler command
  std::string compiler = this->GetCompilerCommand(lang);
  if (compiler.empty()) {
    return dependencies; // No compiler found
  }
  
  // Get compile flags and include directories
  std::string config = this->GetMakefile()->GetSafeDefinition("CMAKE_BUILD_TYPE");
  if (config.empty()) {
    config = "Release";
  }
  
  std::vector<std::string> compileFlags = this->GetCompileFlags(lang, config);
  std::vector<std::string> includeFlags = this->GetIncludeFlags(lang, config);
  
  // Build compiler command for dependency generation
  std::vector<std::string> command;
  command.push_back(compiler);
  command.push_back("-MM"); // Generate dependencies, no system headers
  
  // Add compile flags
  for (const std::string& flag : compileFlags) {
    if (!flag.empty()) {
      command.push_back(flag);
    }
  }
  
  // Add include flags
  for (const std::string& flag : includeFlags) {
    if (!flag.empty()) {
      command.push_back(flag);
    }
  }
  
  command.push_back(source->GetFullPath());
  
  // Execute compiler to get dependencies
  std::string output;
  std::string error;
  int result;
  
  if (cmSystemTools::RunSingleCommand(command, &output, &error, &result,
                                     nullptr, cmSystemTools::OUTPUT_NONE)) {
    if (result == 0) {
      // Parse compiler output
      dependencies = this->ParseCompilerDependencyOutput(output, source);
    }
  }
  
  return dependencies;
}

std::vector<std::string> cmNixTargetGenerator::GetManualDependencies(
  cmSourceFile const* source) const
{
  std::vector<std::string> dependencies;
  
  // Check for OBJECT_DEPENDS property
  cmValue objectDependsValue = source->GetProperty("OBJECT_DEPENDS");
  if (objectDependsValue) {
    cmExpandList(*objectDependsValue, dependencies);
    
    // Convert to relative paths
    std::string sourceDir = this->GetMakefile()->GetCurrentSourceDirectory();
    for (std::string& dep : dependencies) {
      if (cmSystemTools::FileIsFullPath(dep)) {
        std::string relPath = cmSystemTools::RelativePath(sourceDir, dep);
        if (!relPath.empty()) {
          dep = relPath;
        }
      }
    }
  }
  
  return dependencies;
}

std::vector<std::string> cmNixTargetGenerator::ScanWithRegex(
  cmSourceFile const* source, std::string const& /*lang*/) const
{
  std::vector<std::string> dependencies;
  
  // Simple regex-based scanner (fallback for legacy compilers)
  std::ifstream sourceFile(source->GetFullPath());
  if (!sourceFile.is_open()) {
    return dependencies;
  }
  
  // Regex to match #include statements
  std::regex includePattern(R"(^\s*#\s*include\s*[<"]([^">]+)[">])");
  std::string line;
  
  while (std::getline(sourceFile, line)) {
    std::smatch match;
    if (std::regex_search(line, match, includePattern)) {
      std::string headerName = match[1].str();
      
      // Try to resolve include to full path
      std::string fullPath = this->ResolveIncludePath(headerName);
      if (!fullPath.empty()) {
        // Convert to relative path
        std::string sourceDir = this->GetMakefile()->GetCurrentSourceDirectory();
        std::string relPath = cmSystemTools::RelativePath(sourceDir, fullPath);
        dependencies.push_back(!relPath.empty() ? relPath : fullPath);
      }
    }
  }
  
  return dependencies;
}

std::string cmNixTargetGenerator::GetCompilerCommand(std::string const& lang) const
{
  std::string compilerVar = "CMAKE_" + lang + "_COMPILER";
  return this->GetMakefile()->GetSafeDefinition(compilerVar);
}

std::vector<std::string> cmNixTargetGenerator::GetCompileFlags(
  std::string const& lang, std::string const& config) const
{
  std::vector<std::string> flags;
  
  // Get language-specific flags
  std::string langFlags = this->GetMakefile()->GetSafeDefinition("CMAKE_" + lang + "_FLAGS");
  if (!langFlags.empty()) {
    cmExpandList(langFlags, flags);
  }
  
  // Get configuration-specific flags
  std::string configFlags = this->GetMakefile()->GetSafeDefinition(
    "CMAKE_" + lang + "_FLAGS_" + cmSystemTools::UpperCase(config));
  if (!configFlags.empty()) {
    cmExpandList(configFlags, flags);
  }
  
  return flags;
}

std::vector<std::string> cmNixTargetGenerator::GetIncludeFlags(
  std::string const& lang, std::string const& config) const
{
  std::vector<std::string> flags;
  
  // Get include directories from target
  std::vector<BT<std::string>> includesBT = this->GeneratorTarget->GetIncludeDirectories(lang, config);
  
  for (const auto& inc : includesBT) {
    flags.push_back("-I" + inc.Value);
  }
  
  return flags;
}

std::vector<std::string> cmNixTargetGenerator::ParseCompilerDependencyOutput(
  std::string const& output, cmSourceFile const* source) const
{
  std::vector<std::string> dependencies;
  
  // Parse GCC -MM output format: "object: source header1 header2 ..."
  std::istringstream stream(output);
  std::string line;
  
  // Concatenate all lines (dependencies can be split across lines with \)
  std::string fullOutput;
  while (std::getline(stream, line)) {
    // Remove trailing backslash and concatenate
    if (!line.empty() && line.back() == '\\') {
      line.pop_back();
    }
    fullOutput += line + " ";
  }
  
  // Find the colon that separates object from dependencies
  size_t colonPos = fullOutput.find(':');
  if (colonPos != std::string::npos) {
    // Extract everything after the colon
    std::string depsStr = fullOutput.substr(colonPos + 1);
    
    // Split by whitespace to get individual files
    std::istringstream depsStream(depsStr);
    std::string depFile;
    
    while (depsStream >> depFile) {
      // Skip the source file itself, only add headers
      if (depFile != source->GetFullPath() && !depFile.empty()) {
        // Convert to relative path
        std::string sourceDir = this->GetMakefile()->GetCurrentSourceDirectory();
        std::string relPath = cmSystemTools::RelativePath(sourceDir, depFile);
        dependencies.push_back(!relPath.empty() ? relPath : depFile);
      }
    }
  }
  
  return dependencies;
}

std::string cmNixTargetGenerator::ResolveIncludePath(std::string const& headerName) const
{
  // Try to resolve include file to full path using include directories
  std::vector<BT<std::string>> includesBT = this->GeneratorTarget->GetIncludeDirectories("", "");
  
  for (const auto& inc : includesBT) {
    std::string fullPath = inc.Value + "/" + headerName;
    if (cmSystemTools::FileExists(fullPath)) {
      return fullPath;
    }
  }
  
  // Try relative to source directory
  std::string sourceDir = this->GetMakefile()->GetCurrentSourceDirectory();
  std::string fullPath = sourceDir + "/" + headerName;
  if (cmSystemTools::FileExists(fullPath)) {
    return fullPath;
  }
  
  return ""; // Not found
}

void cmNixTargetGenerator::AddIncludeFlags(std::string& flags, 
                                          std::string const& lang,
                                          std::string const& config)
{
  // TODO: Implement include flags for Nix derivations
  // This is Phase 1 core functionality
  (void)flags;
  (void)lang;
  (void)config;
}

std::string cmNixTargetGenerator::GetClangTidyReplacementsFilePath(
  std::string const& directory, cmSourceFile const& source,
  std::string const& config) const
{
  // TODO: Implement clang-tidy replacements file path for Nix
  // This is Phase 1 core functionality
  (void)directory;
  (void)source;
  (void)config;
  return "";
}

// Pure Nix library support implementation
std::vector<std::string> cmNixTargetGenerator::GetTargetLibraryDependencies(
  std::string const& config) const
{
  std::vector<std::string> nixPackages;
  
  // Get link implementation (direct library dependencies)
  auto linkImpl = this->GeneratorTarget->GetLinkImplementation(config, 
    cmGeneratorTarget::UseTo::Compile);
  
  if (!linkImpl) {
    return nixPackages; // No dependencies
  }
  
  // Process each library dependency
  for (const cmLinkItem& item : linkImpl->Libraries) {
    if (item.Target) {
      // This is a CMake target dependency - handle separately
      continue;
    }
    
    // This is an external library (from find_package() or manual specification)
    std::string libName = item.AsStr();
    
    // Find or create Nix package for this library
    std::string nixPackage = this->FindOrCreateNixPackage(libName);
    if (!nixPackage.empty()) {
      nixPackages.push_back(nixPackage);
    }
  }
  
  return nixPackages;
}

std::string cmNixTargetGenerator::FindOrCreateNixPackage(
  std::string const& libName) const
{
  // Try to find existing pkg_<name>.nix file
  std::string nixFile = this->GetNixPackageFilePath(libName);
  
  if (cmSystemTools::FileExists(nixFile)) {
    // Package file exists, return relative path for Nix import
    std::string sourceDir = this->GetMakefile()->GetCurrentSourceDirectory();
    return "./" + cmSystemTools::RelativePath(sourceDir, nixFile);
  }
  
  // File doesn't exist, try to auto-generate it
  if (this->CreateNixPackageFile(libName, nixFile)) {
    std::string sourceDir = this->GetMakefile()->GetCurrentSourceDirectory();
    return "./" + cmSystemTools::RelativePath(sourceDir, nixFile);
  }
  
  return ""; // Could not find or create package
}

std::string cmNixTargetGenerator::GetNixPackageFilePath(
  std::string const& libName) const
{
  std::string sourceDir = this->GetMakefile()->GetCurrentSourceDirectory();
  return sourceDir + "/pkg_" + libName + ".nix";
}

bool cmNixTargetGenerator::CreateNixPackageFile(
  std::string const& libName, std::string const& filePath) const
{
  // Try to map to known Nix package first
  std::string nixPackage = this->MapCommonLibraryToNixPackage(libName);
  
  if (nixPackage.empty()) {
    // Can't auto-generate for unknown library
    return false;
  }
  
  // Generate pkg_<name>.nix file content
  std::ostringstream content;
  content << "# Auto-generated Nix package for " << libName << "\n";
  content << "{ pkgs ? import <nixpkgs> {} }:\n\n";
  content << "pkgs." << nixPackage << "\n";
  
  // Write file
  std::ofstream file(filePath);
  if (!file.is_open()) {
    return false;
  }
  
  file << content.str();
  file.close();
  
  return true;
}

std::string cmNixTargetGenerator::MapCommonLibraryToNixPackage(
  std::string const& libName) const
{
  // Map common CMake imported targets to Nix packages
  static const std::map<std::string, std::string> commonMappings = {
    // OpenGL
    {"OpenGL::GL", "libGL"},
    {"OpenGL::GLU", "libGLU"},
    {"OpenGL::GLEW", "glew"},
    {"GLFW", "glfw"},
    
    // Math and system libraries
    {"m", "glibc"}, // math library
    {"pthread", "glibc"}, // pthread library
    {"dl", "glibc"}, // dynamic linking library
    {"rt", "glibc"}, // real-time library
    
    // Common development libraries
    {"z", "zlib"},
    {"png", "libpng"},
    {"jpeg", "libjpeg"},
    {"ssl", "openssl"},
    {"crypto", "openssl"},
    
    // Audio/Video
    {"SDL2", "SDL2"},
    {"SDL2_image", "SDL2_image"},
    {"SDL2_mixer", "SDL2_mixer"},
    {"SDL2_ttf", "SDL2_ttf"},
    
    // Network
    {"curl", "curl"},
    
    // Database
    {"sqlite3", "sqlite"},
    
    // Development tools
    {"boost", "boost"},
    {"protobuf", "protobuf"},
  };
  
  auto it = commonMappings.find(libName);
  if (it != commonMappings.end()) {
    return it->second;
  }
  
  // Try some heuristics for common patterns
  if (libName.find("::") != std::string::npos) {
    // CMake imported target format (e.g., "MyLib::MyLib")
    size_t pos = libName.find("::");
    std::string baseName = libName.substr(0, pos);
    std::transform(baseName.begin(), baseName.end(), baseName.begin(), ::tolower);
    return baseName;
  }
  
  // Default: assume library name maps directly to Nix package name
  std::string mapped = libName;
  std::transform(mapped.begin(), mapped.end(), mapped.begin(), ::tolower);
  return mapped;
} 