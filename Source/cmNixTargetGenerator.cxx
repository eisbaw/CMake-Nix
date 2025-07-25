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
  cmGlobalNixGenerator* globalGenerator = static_cast<cmGlobalNixGenerator*>(this->GetLocalGenerator()->GetGlobalGenerator());

  for (cmSourceFile* source : sources) {
    // Process C/C++/Fortran/CUDA/Swift/ASM source files
    std::string const& lang = source->GetLanguage();
    if (lang == "C" || lang == "CXX" || lang == "Fortran" || lang == "CUDA" || lang == "Swift" || lang == "ASM" || lang == "ASM-ATT" || lang == "ASM_NASM" || lang == "ASM_MASM") {
      std::vector<std::string> dependencies = this->GetSourceDependencies(source);
      globalGenerator->AddObjectDerivation(this->GetTargetName(), this->GetDerivationName(source), source->GetFullPath(), this->GetObjectFileName(source), lang, dependencies);
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
    if (lang == "C" || lang == "CXX" || lang == "Fortran" || lang == "CUDA" || lang == "Swift" || lang == "ASM" || lang == "ASM-ATT" || lang == "ASM_NASM" || lang == "ASM_MASM") {
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
      
      // Now recursively scan all header dependencies for transitive includes
      std::set<std::string> visited;
      std::vector<std::string> transitiveDeps;
      
      // Mark source file as visited
      visited.insert(source->GetFullPath());
      
      // Process each direct dependency
      for (const std::string& dep : dependencies) {
        // Add the direct dependency
        transitiveDeps.push_back(dep);
        
        // Get absolute path for the dependency
        std::string absPath;
        if (cmSystemTools::FileIsFullPath(dep)) {
          absPath = dep;
        } else {
          absPath = this->GetMakefile()->GetHomeDirectory() + "/" + dep;
        }
        
        // Get transitive dependencies
        std::vector<std::string> transDeps = this->GetTransitiveDependencies(absPath, visited);
        transitiveDeps.insert(transitiveDeps.end(), transDeps.begin(), transDeps.end());
      }
      
      // Remove duplicates while preserving order
      std::set<std::string> seen;
      std::vector<std::string> uniqueDeps;
      for (const std::string& dep : transitiveDeps) {
        if (seen.insert(dep).second) {
          uniqueDeps.push_back(dep);
        }
      }
      
      
      return uniqueDeps;
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
  
  // Add compile flags (but skip optimization flags for dependency scanning)
  for (const std::string& flag : compileFlags) {
    if (!flag.empty() && flag.find("-O") != 0) {
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
    
    // Convert to relative paths from top-level source directory (for Nix generation)
    std::string topSourceDir = this->GetMakefile()->GetHomeDirectory();
    for (std::string& dep : dependencies) {
      if (cmSystemTools::FileIsFullPath(dep)) {
        std::string relPath = cmSystemTools::RelativePath(topSourceDir, dep);
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
        // Convert to relative path from top-level source directory (for Nix generation)
        std::string topSourceDir = this->GetMakefile()->GetHomeDirectory();
        std::string relPath = cmSystemTools::RelativePath(topSourceDir, fullPath);
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
  
  // Get include directories from target using LocalGenerator for proper generator expression evaluation
  std::vector<std::string> includes;
  this->LocalGenerator->GetIncludeDirectories(includes, this->GeneratorTarget, lang, config);
  
  for (const auto& inc : includes) {
    flags.push_back("-I" + inc);
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
        // Convert to relative path from top-level source directory (for Nix generation)
        std::string topSourceDir = this->GetMakefile()->GetHomeDirectory();
        std::string relPath = cmSystemTools::RelativePath(topSourceDir, depFile);
        dependencies.push_back(!relPath.empty() ? relPath : depFile);
      }
    }
  }
  
  return dependencies;
}

std::string cmNixTargetGenerator::ResolveIncludePath(std::string const& headerName) const
{
  // Try to resolve include file to full path using include directories
  std::vector<std::string> includes;
  // Use empty strings for lang and config to get all include directories
  this->LocalGenerator->GetIncludeDirectories(includes, this->GeneratorTarget, "", "");
  
  for (const auto& inc : includes) {
    std::string fullPath = inc + "/" + headerName;
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
  cmLinkImplementation const* linkImpl = this->GeneratorTarget->GetLinkImplementation(config, cmGeneratorTarget::UseTo::Compile);
  
  if (!linkImpl) {
    return nixPackages; // No dependencies
  }
  
  // Process each library dependency
  for (const cmLinkItem& item : linkImpl->Libraries) {
    if (item.Target && item.Target->IsImported()) {
        std::string targetName = item.Target->GetName();
        std::string nixPackage = this->PackageMapper.GetNixPackageForTarget(targetName);
        if (!nixPackage.empty()) {
          nixPackages.push_back("__NIXPKG__" + nixPackage);
        }
    } else if (!item.Target) {
        std::string libName = item.AsStr();
        std::string nixPackage = this->FindOrCreateNixPackage(libName);
        if (!nixPackage.empty()) {
          nixPackages.push_back(nixPackage);
        }
    }
  }
  
  return nixPackages;
}

std::string cmNixTargetGenerator::FindOrCreateNixPackage(
  std::string const& libName) const
{
  // Try to find existing pkg_<name>.nix file
  std::string nixFile = this->PackageMapper.GetNixPackageForTarget(libName);
  if (nixFile.empty()) {
    return "";
  }

  std::string nixFilePath = this->GetMakefile()->GetCurrentSourceDirectory() + "/pkg_" + nixFile + ".nix";
  
  if (cmSystemTools::FileExists(nixFilePath)) {
    // Package file exists, return relative path for Nix import
    std::string sourceDir = this->GetMakefile()->GetCurrentSourceDirectory();
    return "./" + cmSystemTools::RelativePath(sourceDir, nixFilePath);
  }
  
  // File doesn't exist, try to auto-generate it
  if (this->CreateNixPackageFile(libName, nixFilePath)) {
    std::string sourceDir = this->GetMakefile()->GetCurrentSourceDirectory();
    return "./" + cmSystemTools::RelativePath(sourceDir, nixFilePath);
  }
  
  return ""; // Could not find or create package
}

bool cmNixTargetGenerator::CreateNixPackageFile(
  std::string const& libName, std::string const& filePath) const
{
  // Try to map to known Nix package first
  std::string nixPackage = this->PackageMapper.GetNixPackageForTarget(libName);
  
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

std::vector<std::string> cmNixTargetGenerator::GetTransitiveDependencies(
  std::string const& filePath, std::set<std::string>& visited) const
{
  std::vector<std::string> dependencies;
  
  // Check if already visited
  if (!visited.insert(filePath).second) {
    return dependencies; // Already processed this file
  }
  
  // Check if file exists
  if (!cmSystemTools::FileExists(filePath)) {
    return dependencies;
  }
  
  // Determine the language based on file extension
  std::string ext = cmSystemTools::GetFilenameLastExtension(filePath);
  std::string lang;
  if (ext == ".h" || ext == ".hpp" || ext == ".hxx" || ext == ".H" || 
      ext == ".hh" || ext == ".h++" || ext == ".hp") {
    lang = "CXX"; // Treat all headers as C++ for scanning
  } else if (ext == ".c") {
    lang = "C";
  } else if (ext == ".cuh") {
    lang = "CUDA";
  } else {
    return dependencies; // Unknown file type
  }
  
  // Use compiler to scan this header file
  std::vector<std::string> directDeps;
  
  // Get compiler command
  std::string compiler = this->GetCompilerCommand(lang);
  if (!compiler.empty()) {
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
    command.push_back("-MT"); // Specify target name
    command.push_back("dummy"); // Dummy target name
    
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
    
    command.push_back(filePath);
    
    // Execute compiler to get dependencies
    std::string output;
    std::string error;
    int result;
    
    if (cmSystemTools::RunSingleCommand(command, &output, &error, &result,
                                       nullptr, cmSystemTools::OUTPUT_NONE)) {
      if (result == 0) {
        // Parse compiler output
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
            // Skip the header file itself, only add its dependencies
            if (depFile != filePath && !depFile.empty()) {
              // Convert to relative path from top-level source directory (for Nix generation)
              std::string topSourceDir = this->GetMakefile()->GetHomeDirectory();
              std::string relPath = cmSystemTools::RelativePath(topSourceDir, depFile);
              directDeps.push_back(!relPath.empty() ? relPath : depFile);
            }
          }
        }
      }
    }
  }
  
  // Fallback to regex scanning if compiler method fails
  if (directDeps.empty()) {
    std::ifstream headerFile(filePath);
    if (headerFile.is_open()) {
      // Regex to match #include statements
      std::regex includePattern(R"(^\s*#\s*include\s*[<"]([^">]+)[">])");
      std::string line;
      
      while (std::getline(headerFile, line)) {
        std::smatch match;
        if (std::regex_search(line, match, includePattern)) {
          std::string headerName = match[1].str();
          
          // Try to resolve include to full path
          std::string fullPath = this->ResolveIncludePath(headerName);
          if (!fullPath.empty()) {
            // Convert to relative path from top-level source directory (for Nix generation)
            std::string topSourceDir = this->GetMakefile()->GetHomeDirectory();
            std::string relPath = cmSystemTools::RelativePath(topSourceDir, fullPath);
            directDeps.push_back(!relPath.empty() ? relPath : fullPath);
          }
        }
      }
    }
  }
  
  // Process each direct dependency recursively
  for (const std::string& dep : directDeps) {
    // Add the direct dependency
    dependencies.push_back(dep);
    
    // Get absolute path for the dependency
    std::string absPath;
    if (cmSystemTools::FileIsFullPath(dep)) {
      absPath = dep;
    } else {
      absPath = this->GetMakefile()->GetHomeDirectory() + "/" + dep;
    }
    
    // Get transitive dependencies
    std::vector<std::string> transDeps = this->GetTransitiveDependencies(absPath, visited);
    dependencies.insert(dependencies.end(), transDeps.begin(), transDeps.end());
  }
  
  return dependencies;
} 