/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */
#include "cmNixTargetGenerator.h"

#include <cm/memory>
#include <sstream>
#include <algorithm>
#include <set>
#include <unordered_set>
#include <system_error>
#include <exception>

#include "cmGeneratorTarget.h"
#include "cmGlobalNixGenerator.h"
#include "cmLocalNixGenerator.h"
#include "cmNixCacheManager.h"
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
#include "cmake.h"
#include <regex>
#include <fstream>

std::unique_ptr<cmNixTargetGenerator> cmNixTargetGenerator::New(
  cmGeneratorTarget* target)
{
  return std::make_unique<cmNixTargetGenerator>(target);
}

cmNixTargetGenerator::cmNixTargetGenerator(cmGeneratorTarget* target)
  : cmCommonTargetGenerator(target)
  , LocalGenerator(
      static_cast<cmLocalNixGenerator*>(target->GetLocalGenerator()))
{
}

cmNixTargetGenerator::~cmNixTargetGenerator() = default;

void cmNixTargetGenerator::LogDebug(const std::string& message) const
{
  if (this->GetMakefile()->GetCMakeInstance()->GetDebugOutput()) {
    std::ostringstream oss;
    oss << "[NIX-DEBUG] " << message;
    cmSystemTools::Message(oss.str());
  }
}

bool cmNixTargetGenerator::IsCompilableLanguage(const std::string& lang) const
{
  return lang == "C" || lang == "CXX" || lang == "Fortran" || lang == "CUDA" || 
         lang == "Swift" || lang == "ASM" || lang == "ASM-ATT" || 
         lang == "ASM_NASM" || lang == "ASM_MASM";
}

void cmNixTargetGenerator::Generate()
{
  // Generate precompiled header derivations if needed
  this->WritePchDerivations();
  
  // Generate per-source file derivations
  this->WriteObjectDerivations();
  
  // Generate linking derivation
  this->WriteLinkDerivation();
}

std::string const& cmNixTargetGenerator::GetTargetName() const
{
  return this->GeneratorTarget->GetName();
}

void cmNixTargetGenerator::WriteObjectDerivations()
{
  // Get build configuration  
  std::string config = this->GetMakefile()->GetSafeDefinition("CMAKE_BUILD_TYPE");
  if (config.empty()) {
    config = "Release";
  }

  // Get all source files for this target
  std::vector<cmSourceFile*> sources;
  this->GeneratorTarget->GetSourceFiles(sources, config);
  cmGlobalNixGenerator* globalGenerator = static_cast<cmGlobalNixGenerator*>(this->GetLocalGenerator()->GetGlobalGenerator());

  for (cmSourceFile* source : sources) {
    // Process C/C++/Fortran/CUDA/Swift/ASM source files
    std::string const& lang = source->GetLanguage();
    if (this->IsCompilableLanguage(lang)) {
      std::vector<std::string> dependencies = this->GetSourceDependencies(source);
      
      // Add PCH dependencies if applicable
      std::vector<std::string> pchDeps = this->GetPchDependencies(source, config);
      dependencies.insert(dependencies.end(), pchDeps.begin(), pchDeps.end());
      
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
    if (this->IsCompilableLanguage(lang)) {
      objectDeps.push_back(this->GetDerivationName(source));
    }
  }
  
  // Nix derivation generation is handled by cmGlobalNixGenerator
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
  
  // Skip dependency scanning unless CMAKE_NIX_EXPLICIT_SOURCES is enabled
  // This avoids redundant compiler invocations during configuration
  // Note: This means header files won't be automatically included in filesets,
  // so projects need to use src = ./. or CMAKE_NIX_EXPLICIT_SOURCES=ON
  cmValue explicitSources = this->GetMakefile()->GetDefinition("CMAKE_NIX_EXPLICIT_SOURCES");
  if (!explicitSources || !cmIsOn(*explicitSources)) {
    return dependencies; // Skip scanning to improve performance
  }
  
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
        std::vector<std::string> transDeps = this->GetTransitiveDependencies(absPath, visited, 0);
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
  } catch (const std::bad_alloc& e) {
    // Handle out of memory specifically
    std::ostringstream msg;
    msg << "Out of memory during dependency scanning for " << source->GetFullPath();
    this->Makefile->GetCMakeInstance()->IssueMessage(MessageType::WARNING, msg.str());
    this->LogDebug(msg.str());
    // Fall back to other methods if compiler scanning fails
  } catch (const std::system_error& e) {
    // Handle system errors (file I/O, process execution)
    std::ostringstream msg;
    msg << "System error during dependency scanning for " << source->GetFullPath() 
        << ": " << e.what() << " (code: " << e.code() << ")";
    this->Makefile->GetCMakeInstance()->IssueMessage(MessageType::WARNING, msg.str());
    this->LogDebug(msg.str());
    // Fall back to other methods if compiler scanning fails
  } catch (const std::runtime_error& e) {
    // Handle runtime errors (command execution failures)
    std::ostringstream msg;
    msg << "Runtime error during dependency scanning for " << source->GetFullPath() 
        << ": " << e.what();
    this->Makefile->GetCMakeInstance()->IssueMessage(MessageType::WARNING, msg.str());
    this->LogDebug(msg.str());
    // Fall back to other methods if compiler scanning fails
  } catch (const std::exception& e) {
    // Handle any other standard exceptions
    std::ostringstream msg;
    msg << "Exception during dependency scanning for " << source->GetFullPath() 
        << ": " << e.what();
    this->Makefile->GetCMakeInstance()->IssueMessage(MessageType::WARNING, msg.str());
    this->LogDebug(msg.str());
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
  
  // Debug output before running command
  if (this->GetMakefile()->GetCMakeInstance()->GetDebugOutput()) {
    this->LogDebug("ScanWithCompiler for " + source->GetFullPath());
    
    // Show raw compileFlags vector
    std::ostringstream flagsMsg;
    flagsMsg << "Raw compileFlags (" << compileFlags.size() << " flags):";
    this->LogDebug(flagsMsg.str());
    for (size_t i = 0; i < compileFlags.size(); ++i) {
      std::ostringstream flagMsg;
      flagMsg << "  [" << i << "] = \"" << compileFlags[i] << "\"";
      this->LogDebug(flagMsg.str());
    }
    
    // Show raw includeFlags vector
    flagsMsg.str("");
    flagsMsg << "Raw includeFlags (" << includeFlags.size() << " flags):";
    this->LogDebug(flagsMsg.str());
    for (size_t i = 0; i < includeFlags.size(); ++i) {
      std::ostringstream flagMsg;
      flagMsg << "  [" << i << "] = \"" << includeFlags[i] << "\"";
      this->LogDebug(flagMsg.str());
    }
    
    // Show full command being executed
    this->LogDebug("Full dependency scan command:");
    std::ostringstream cmdMsg;
    cmdMsg << "  ";
    for (const auto& arg : command) {
      cmdMsg << "\"" << arg << "\" ";
    }
    this->LogDebug(cmdMsg.str());
  }
  
  // Execute compiler to get dependencies
  std::string output;
  std::string error;
  int result;
  
  if (cmSystemTools::RunSingleCommand(command, &output, &error, &result,
                                     nullptr, cmSystemTools::OUTPUT_NONE)) {
    if (result == 0) {
      // Parse compiler output
      dependencies = this->ParseCompilerDependencyOutput(output, source);
    } else {
      // Always report compiler errors, not just in debug mode
      std::ostringstream msg;
      msg << "Compiler dependency scan failed for " << source->GetFullPath() 
          << " with exit code " << result;
      if (!error.empty()) {
        msg << ": " << error;
      }
      this->Makefile->GetCMakeInstance()->IssueMessage(
        MessageType::WARNING, msg.str());
      
      // Additional debug output for failed commands
      if (this->GetMakefile()->GetCMakeInstance()->GetDebugOutput()) {
        this->LogDebug("Dependency scan command failed!");
        this->LogDebug("Exit code: " + std::to_string(result));
        this->LogDebug("Error output: " + error);
        this->LogDebug("Standard output: " + output);
      }
    }
  } else {
    // Always report command execution failures
    std::ostringstream msg;
    msg << "Failed to execute dependency scanning command for " 
        << source->GetFullPath();
    if (!error.empty()) {
      msg << ": " << error;
    }
    this->Makefile->GetCMakeInstance()->IssueMessage(
      MessageType::WARNING, msg.str());
    
    // Additional debug output for command execution failure
    if (this->GetMakefile()->GetCMakeInstance()->GetDebugOutput()) {
      this->LogDebug("Failed to execute dependency scan command!");
      this->LogDebug("Error: " + error);
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
  
  // No need to explicitly close - RAII handles it
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
    // Trim leading/trailing spaces before expanding
    langFlags = cmTrimWhitespace(langFlags);
    // Use ParseUnixCommandLine to properly handle quoted arguments
    std::vector<std::string> parsedFlags;
    cmSystemTools::ParseUnixCommandLine(langFlags.c_str(), parsedFlags);
    flags.insert(flags.end(), parsedFlags.begin(), parsedFlags.end());
  }
  
  // Get configuration-specific flags
  std::string configFlags = this->GetMakefile()->GetSafeDefinition(
    "CMAKE_" + lang + "_FLAGS_" + cmSystemTools::UpperCase(config));
  if (!configFlags.empty()) {
    // Trim leading/trailing spaces before expanding
    configFlags = cmTrimWhitespace(configFlags);
    // Use ParseUnixCommandLine to properly handle quoted arguments
    std::vector<std::string> parsedFlags;
    cmSystemTools::ParseUnixCommandLine(configFlags.c_str(), parsedFlags);
    flags.insert(flags.end(), parsedFlags.begin(), parsedFlags.end());
  }
  
  // Get target-specific compile definitions
  std::set<std::string> defines;
  this->LocalGenerator->GetTargetDefines(this->GeneratorTarget, config, lang, defines);
  for (const std::string& define : defines) {
    flags.push_back("-D" + define);
  }
  
  // Get target-specific compile options (including those from COMPILE_LANGUAGE expressions)
  std::vector<BT<std::string>> compileOpts = this->LocalGenerator->GetTargetCompileFlags(
    this->GeneratorTarget, config, lang, "");
  for (const auto& opt : compileOpts) {
    if (!opt.Value.empty()) {
      std::string trimmedOpt = cmTrimWhitespace(opt.Value);
      
      // Check if the entire string is wrapped in quotes
      if (trimmedOpt.length() >= 2 && 
          trimmedOpt.front() == '"' && trimmedOpt.back() == '"') {
        // Remove the outer quotes
        trimmedOpt = trimmedOpt.substr(1, trimmedOpt.length() - 2);
      }
      
      // Use ParseUnixCommandLine to properly handle quoted arguments
      std::vector<std::string> parsedFlags;
      cmSystemTools::ParseUnixCommandLine(trimmedOpt.c_str(), parsedFlags);
      
      // If ParseUnixCommandLine returns a single flag that contains spaces,
      // it might need to be split further (unless it's a quoted argument)
      std::vector<std::string> finalFlags;
      for (const auto& flag : parsedFlags) {
        if (flag.find(' ') != std::string::npos && 
            flag.front() != '"' && flag.front() != '\'') {
          // This flag contains spaces but isn't quoted, so split it
          std::istringstream iss(flag);
          std::string subflag;
          while (iss >> subflag) {
            if (!subflag.empty()) {
              finalFlags.push_back(subflag);
            }
          }
        } else {
          finalFlags.push_back(flag);
        }
      }
      
      flags.insert(flags.end(), finalFlags.begin(), finalFlags.end());
    }
  }
  
  // Remove any empty or whitespace-only flags
  flags.erase(std::remove_if(flags.begin(), flags.end(),
    [](const std::string& flag) {
      return flag.empty() || std::all_of(flag.begin(), flag.end(), ::isspace);
    }), flags.end());
  
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
  std::ostringstream fullOutputStream;
  while (std::getline(stream, line)) {
    // Remove trailing backslash and concatenate
    if (!line.empty() && line.back() == '\\') {
      line.pop_back();
    }
    fullOutputStream << line << " ";
  }
  std::string fullOutput = fullOutputStream.str();
  
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
  // Include flags are added directly in the build phase of derivations
  (void)flags;
  (void)lang;
  (void)config;
}

std::string cmNixTargetGenerator::GetClangTidyReplacementsFilePath(
  std::string const& directory, cmSourceFile const& source,
  std::string const& config) const
{
  // Generate clang-tidy replacements file path similar to other generators
  std::string filename = cmSystemTools::GetFilenameName(source.GetFullPath());
  std::string basename = cmSystemTools::GetFilenameWithoutLastExtension(filename);
  
  // Create path: <directory>/<config>/<basename>.yaml
  std::string replacementsFile = directory;
  if (!config.empty()) {
    replacementsFile = cmStrCat(replacementsFile, "/", config);
  }
  replacementsFile = cmStrCat(replacementsFile, "/", basename, ".yaml");
  
  return replacementsFile;
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
    if (item.Target) {
      // Handle CMake targets (both internal and imported)
      if (item.Target->IsImported()) {
        // Handle imported targets with package mapping
        std::string targetName = item.Target->GetName();
        std::string nixPackage = cmNixPackageMapper::GetInstance().GetNixPackageForTarget(targetName);
        if (!nixPackage.empty()) {
          nixPackages.push_back("__NIXPKG__" + nixPackage);
        }
        // If no mapping found, fall through to handle as external library
        else {
          std::string libName = item.AsStr();
          std::string externalPkg = this->FindOrCreateNixPackage(libName);
          if (!externalPkg.empty()) {
            nixPackages.push_back(externalPkg);
          }
        }
      } else {
        // Internal project targets - these are handled as Nix derivation dependencies
        // They don't need to be in nixPackages, they're handled elsewhere
        continue;
      }
    } else {
      // External libraries (not CMake targets)
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
  // Skip linker flags - they should not be treated as packages
  if (libName.find("-Wl,") == 0 || libName.find("-l") == 0 || 
      libName.find("-L") == 0 || libName.find("-framework") == 0) {
    return "";
  }
  
  // Try to find existing pkg_<name>.nix file
  std::string nixFile = cmNixPackageMapper::GetInstance().GetNixPackageForTarget(libName);
  if (nixFile.empty()) {
    return "";
  }

  // Sanitize the filename - replace problematic characters with underscore
  std::string sanitizedNixFile = nixFile;
  for (char& c : sanitizedNixFile) {
    if (c == ',' || c == ' ' || c == '(' || c == ')' || c == '\'' || c == '"') {
      c = '_';
    }
  }

  // First check in the current source directory
  std::string nixFilePath = this->GetMakefile()->GetCurrentSourceDirectory() + "/pkg_" + sanitizedNixFile + ".nix";
  
  // If not found, check in parent directories
  if (!cmSystemTools::FileExists(nixFilePath)) {
    // Try project source directory
    std::string projectDir = this->GetMakefile()->GetHomeDirectory();
    nixFilePath = projectDir + "/pkg_" + sanitizedNixFile + ".nix";
  }
  
  if (cmSystemTools::FileExists(nixFilePath)) {
    // Package file exists, return relative path for Nix import
    std::string sourceDir = this->GetMakefile()->GetCurrentSourceDirectory();
    std::string relPath = cmSystemTools::RelativePath(sourceDir, nixFilePath);
    
    // If RelativePath returns an absolute path (starting with /), it's likely
    // because RelativePath failed. Just compute a simple relative path.
    if (!relPath.empty() && relPath[0] == '/') {
      // This means RelativePath failed - just use basename with parent nav
      std::string basename = cmSystemTools::GetFilenameName(nixFilePath);
      
      // Assume it's in the parent directory
      return "./../../" + basename;
    }
    
    // Check if the relative path makes sense
    if (relPath.find("..") == std::string::npos && nixFilePath.find(sourceDir) == std::string::npos) {
      // The file is not in a subdirectory and relPath doesn't navigate up
      // This suggests the file is in a parent directory
      std::string basename = cmSystemTools::GetFilenameName(nixFilePath);
      return "./../../" + basename;
    }
    
    return "./" + relPath;
  }
  
  // File doesn't exist, try to auto-generate it
  if (this->CreateNixPackageFile(libName, nixFilePath)) {
    std::string sourceDir = this->GetMakefile()->GetCurrentSourceDirectory();
    std::string relPath = cmSystemTools::RelativePath(sourceDir, nixFilePath);
    
    // Handle the case where the file was created in a parent directory
    if (!relPath.empty() && relPath[0] == '/') {
      // This means RelativePath failed - just use basename with parent nav
      std::string basename = cmSystemTools::GetFilenameName(nixFilePath);
      return "./../../" + basename;
    }
    
    // Check if the relative path makes sense
    if (relPath.find("..") == std::string::npos && nixFilePath.find(sourceDir) == std::string::npos) {
      // The file is not in a subdirectory and relPath doesn't navigate up
      // This suggests the file is in a parent directory
      std::string basename = cmSystemTools::GetFilenameName(nixFilePath);
      return "./../../" + basename;
    }
    
    return "./" + relPath;
  } else {
    // Issue warning when package file creation fails
    std::ostringstream msg;
    msg << "Warning: Could not create Nix package file for library '" << libName 
        << "' at '" << nixFilePath << "'";
    this->Makefile->GetCMakeInstance()->IssueMessage(MessageType::WARNING, msg.str());
  }
  
  return ""; // Could not find or create package
}

bool cmNixTargetGenerator::CreateNixPackageFile(
  std::string const& libName, std::string const& filePath) const
{
  // Skip linker flags - they should not be treated as packages
  if (libName.find("-Wl,") == 0 || libName.find("-l") == 0 || 
      libName.find("-L") == 0 || libName.find("-framework") == 0) {
    return false;
  }
  
  // Try to map to known Nix package first
  std::string nixPackage = cmNixPackageMapper::GetInstance().GetNixPackageForTarget(libName);
  
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
  {
    std::ofstream file(filePath);
    if (!file.is_open()) {
      return false;
    }
    
    file << content.str();
    // File is automatically closed when it goes out of scope
  }
  
  // Verify file was written successfully
  if (!cmSystemTools::FileExists(filePath)) {
    std::ostringstream msg;
    msg << "Failed to write file: " << filePath;
    this->Makefile->GetCMakeInstance()->IssueMessage(MessageType::WARNING, msg.str());
    return false;
  }
  
  return true;
}

std::vector<std::string> cmNixTargetGenerator::GetTransitiveDependencies(
  std::string const& filePath, std::set<std::string>& visited, int depth) const
{
  std::vector<std::string> dependencies;
  
  // Limit recursion depth to prevent stack overflow
  if (depth > MAX_HEADER_RECURSION_DEPTH) {
    this->GetMakefile()->IssueMessage(
      MessageType::WARNING,
      cmStrCat("Header dependency recursion depth exceeded for: ", filePath));
    return dependencies;
  }
  
  // Canonicalize the path to ensure consistent cache keys
  std::string canonicalPath = cmSystemTools::GetRealPath(filePath);
  
  // Check if already visited (using canonical path)
  if (!visited.insert(canonicalPath).second) {
    return dependencies; // Already processed this file
  }
  
  // Check if file exists
  if (!cmSystemTools::FileExists(canonicalPath)) {
    return dependencies;
  }
  
  // Get cache manager from global generator
  auto* globalGen = static_cast<cmGlobalNixGenerator*>(
    this->GetLocalGenerator()->GetGlobalGenerator());
  auto* cacheManager = globalGen->GetCacheManager();
  
  // Use consolidated cache manager with lambda for lazy computation
  return cacheManager->GetTransitiveDependencies(canonicalPath, 
    [&]() -> std::vector<std::string> {
  
  // Determine the language based on file extension
  std::string ext = cmSystemTools::GetFilenameLastExtension(canonicalPath);
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
    
    command.push_back(canonicalPath);
    
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
        std::ostringstream fullOutputStream;
        while (std::getline(stream, line)) {
          // Remove trailing backslash and concatenate
          if (!line.empty() && line.back() == '\\') {
            line.pop_back();
          }
          fullOutputStream << line << " ";
        }
        std::string fullOutput = fullOutputStream.str();
        
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
    } else {
      // Log compiler error without failing the build
      if (this->GetMakefile()->GetCMakeInstance()->GetDebugOutput()) {
        this->LogDebug("Compiler header dependency scan failed for " + filePath +
                      " with exit code " + std::to_string(result));
        if (!error.empty()) {
          this->LogDebug("Compiler error: " + error);
        }
      }
    }
  } else {
    // Log command execution failure
    this->LogDebug("Failed to execute header dependency scanning command for " + filePath);
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
      // No need to explicitly close - RAII handles it
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
    std::vector<std::string> transDeps = this->GetTransitiveDependencies(absPath, visited, depth + 1);
    dependencies.insert(dependencies.end(), transDeps.begin(), transDeps.end());
  }
  
      // Mark all dependencies as visited before returning
      for (const auto& dep : dependencies) {
        visited.insert(dep);
      }
      
      return dependencies;
    }); // End of lambda
}

void cmNixTargetGenerator::WritePchDerivations()
{
  std::string config = this->GetMakefile()->GetSafeDefinition("CMAKE_BUILD_TYPE");
  if (config.empty()) {
    config = "Release";
  }

  cmGlobalNixGenerator* globalGenerator = 
    static_cast<cmGlobalNixGenerator*>(this->GetLocalGenerator()->GetGlobalGenerator());

  // Check if target has precompile headers for each language
  std::set<std::string> languages;
  this->GeneratorTarget->GetLanguages(languages, config);

  for (const std::string& lang : languages) {
    if (!this->NeedsPchSupport(config, lang)) {
      continue;
    }

    // Get PCH architectures for this language
    std::vector<std::string> pchArchs = 
      this->GeneratorTarget->GetPchArchs(config, lang);

    for (const std::string& arch : pchArchs) {
      // Get PCH source file
      std::string pchSource = this->GeneratorTarget->GetPchSource(config, lang, arch);
      if (pchSource.empty()) {
        continue;
      }

      // Get PCH header file
      std::string pchHeader = this->GeneratorTarget->GetPchHeader(config, lang, arch);
      
      // Get PCH object file path
      std::string pchObject = this->GeneratorTarget->GetPchFileObject(config, lang, arch);
      
      // Get the actual PCH file path
      std::string pchFile = this->GeneratorTarget->GetPchFile(config, lang, arch);

      // Create derivation name for PCH
      std::string derivationName = this->GetPchDerivationName(lang, arch);
      
      // Add PCH derivation to global generator
      // The PCH needs to include the header as a dependency
      std::vector<std::string> pchDeps;
      pchDeps.push_back(pchHeader);
      
      // Add the PCH creation derivation
      globalGenerator->AddObjectDerivation(
        this->GetTargetName(), 
        derivationName, 
        pchSource, 
        pchFile,  // Output is the PCH file, not object
        lang, 
        pchDeps
      );
    }
  }
}

std::string cmNixTargetGenerator::GetPchDerivationName(
  std::string const& language, std::string const& arch) const
{
  std::string name = this->GetTargetName() + "_pch_" + language;
  if (!arch.empty()) {
    name += "_" + arch;
  }
  return name;
}

bool cmNixTargetGenerator::NeedsPchSupport(
  std::string const& config, std::string const& language) const
{
  // Suppress unused parameter warning - config may be used in future
  static_cast<void>(config);
  
  // Check if this target has precompile headers for this language
  cmValue pchHeaders = this->GeneratorTarget->GetProperty("PRECOMPILE_HEADERS");
  if (!pchHeaders || pchHeaders->empty()) {
    return false;
  }

  // Check if PCH is disabled for this target
  cmValue disablePch = this->GeneratorTarget->GetProperty("DISABLE_PRECOMPILE_HEADERS");
  if (disablePch && cmIsOn(*disablePch)) {
    return false;
  }

  // Check if the language supports PCH
  if (language != "C" && language != "CXX" && language != "OBJC" && language != "OBJCXX") {
    return false;
  }

  // Check if we have a PCH extension defined for this compiler
  std::string pchExtVar = "CMAKE_" + language + "_COMPILER_PRECOMPILE_HEADER_EXTENSION";
  cmValue pchExt = this->GetMakefile()->GetDefinition(pchExtVar);
  if (!pchExt || pchExt->empty()) {
    return false;
  }

  return true;
}

std::vector<std::string> cmNixTargetGenerator::GetPchDependencies(
  cmSourceFile const* source, std::string const& config) const
{
  std::vector<std::string> pchDeps;
  
  // Skip if this source file has SKIP_PRECOMPILE_HEADERS property
  if (source->GetPropertyAsBool("SKIP_PRECOMPILE_HEADERS")) {
    return pchDeps;
  }

  std::string const& lang = source->GetLanguage();
  if (!this->NeedsPchSupport(config, lang)) {
    return pchDeps;
  }

  // Get PCH architectures
  std::vector<std::string> pchArchs = 
    this->GeneratorTarget->GetPchArchs(config, lang);

  // Check if this source file is a PCH source itself
  std::unordered_set<std::string> pchSources;
  for (const std::string& arch : pchArchs) {
    std::string pchSource = this->GeneratorTarget->GetPchSource(config, lang, arch);
    if (!pchSource.empty()) {
      pchSources.insert(pchSource);
    }
  }

  // If this is not a PCH source, it depends on the PCH files
  if (pchSources.find(source->GetFullPath()) == pchSources.end()) {
    for (const std::string& arch : pchArchs) {
      // Add dependency on the PCH derivation
      std::string pchDerivationName = this->GetPchDerivationName(lang, arch);
      pchDeps.push_back(pchDerivationName);
    }
  }

  return pchDeps;
}