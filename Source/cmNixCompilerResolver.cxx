/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */
#include "cmNixCompilerResolver.h"

#include "cmGlobalGenerator.h"
#include "cmMakefile.h"
#include "cmState.h"
#include "cmStringAlgorithms.h"
#include "cmSystemTools.h"
#include "cmValue.h"
#include "cmake.h"

#include <algorithm>

// Initialize static compiler mappings
const std::unordered_map<std::string, std::string> cmNixCompilerResolver::CompilerIdToPackage = {
  {"GNU", "gcc"},
  {"Clang", "clang"}, 
  {"AppleClang", "clang"},
  {"Intel", "intel-compiler"},
  {"IntelLLVM", "intel-compiler"}, 
  {"MSVC", "msvc"},
  {"PGI", "pgi"},
  {"NVHPC", "nvhpc"},
  {"XL", "xlc"},
  {"XLClang", "xlc"},
  {"Fujitsu", "fujitsu"},
  {"FujitsuClang", "fujitsu"}
};

const std::unordered_map<std::string, std::string> cmNixCompilerResolver::DefaultCommands = {
  {"C_gcc", "gcc"},
  {"C_clang", "clang"},
  {"CXX_gcc", "g++"},
  {"CXX_clang", "clang++"},
  {"Fortran_gcc", "gfortran"},
  {"Fortran_gfortran", "gfortran"},
  {"Fortran_intel-compiler", "ifort"},
  {"CUDA_cudatoolkit", "nvcc"},
  {"Swift_swift", "swiftc"},
  {"ASM_gcc", "gcc"},
  {"ASM_clang", "clang"},
  {"ASM-ATT_gcc", "gcc"},
  {"ASM-ATT_clang", "clang"},
  {"ASM_NASM_nasm", "nasm"},
  {"ASM_MASM_masm", "ml"}
};

cmNixCompilerResolver::cmNixCompilerResolver(cmake* cm)
  : CMakeInstance(cm)
{
}

std::string cmNixCompilerResolver::GetCompilerPackage(const std::string& lang)
{
  std::lock_guard<std::mutex> lock(this->CacheMutex);
  
  auto it = this->CompilerCache.find(lang);
  if (it != this->CompilerCache.end()) {
    return it->second.package;
  }
  
  CompilerInfo info = this->DetectCompiler(lang);
  this->CompilerCache[lang] = info;
  return info.package;
}

std::string cmNixCompilerResolver::GetCompilerCommand(const std::string& lang)
{
  std::lock_guard<std::mutex> lock(this->CacheMutex);
  
  auto it = this->CompilerCache.find(lang);
  if (it != this->CompilerCache.end()) {
    return it->second.command;
  }
  
  CompilerInfo info = this->DetectCompiler(lang);
  this->CompilerCache[lang] = info;
  return info.command;
}

bool cmNixCompilerResolver::SupportsCrossCompilation(const std::string& lang) const
{
  // Currently only GCC and Clang support cross-compilation in our Nix setup
  std::string compilerId = this->GetCompilerId(lang);
  return (compilerId == "GNU" || compilerId == "Clang" || compilerId == "AppleClang");
}

void cmNixCompilerResolver::ClearCache()
{
  std::lock_guard<std::mutex> lock(this->CacheMutex);
  this->CompilerCache.clear();
}

cmNixCompilerResolver::CompilerInfo cmNixCompilerResolver::DetectCompiler(const std::string& lang) const
{
  CompilerInfo info;
  info.supportsCrossCompile = false;
  
  // Check for user override first
  std::string packageOverride = this->GetUserOverride(lang, "_COMPILER_PACKAGE");
  if (!packageOverride.empty()) {
    info.package = packageOverride;
  } else {
    // Special cases for specific languages
    if (lang == "CUDA") {
      info.package = "cudatoolkit";
    } else if (lang == "Swift") {
      info.package = "swift";
    } else if (lang == "Fortran") {
      // Special handling for Fortran
      std::string compilerId = this->GetCompilerId(lang);
      if (compilerId == "GNU") {
        info.package = "gfortran";
      } else if (compilerId == "Intel" || compilerId == "IntelLLVM") {
        info.package = "intel-compiler";
      } else {
        info.package = "gfortran"; // Default Fortran compiler
      }
    } else if (lang == "ASM_NASM") {
      info.package = "nasm";
    } else if (lang == "ASM_MASM") {
      info.package = "masm";
    } else {
      // Use compiler ID mapping
      std::string compilerId = this->GetCompilerId(lang);
      auto idIt = CompilerIdToPackage.find(compilerId);
      if (idIt != CompilerIdToPackage.end()) {
        info.package = idIt->second;
      } else {
        // Try to detect from compiler path
        std::string compilerPath = this->GetCompilerPath(lang);
        if (!compilerPath.empty()) {
          std::string compilerName = cmSystemTools::GetFilenameName(compilerPath);
          std::transform(compilerName.begin(), compilerName.end(), compilerName.begin(), ::tolower);
          
          if (compilerName.find("clang") != std::string::npos) {
            info.package = "clang";
          } else if (compilerName.find("gcc") != std::string::npos || 
                     compilerName.find("g++") != std::string::npos) {
            info.package = "gcc";
          } else if (compilerName.find("icc") != std::string::npos ||
                     compilerName.find("icpc") != std::string::npos) {
            info.package = "intel-compiler";
          } else {
            // Default based on language
            info.package = (lang == "C" || lang == "CXX") ? "gcc" : "gcc";
          }
        } else {
          info.package = "gcc"; // Ultimate fallback
        }
      }
    }
  }
  
  // Check for command override
  std::string commandOverride = this->GetUserOverride(lang, "_COMPILER_COMMAND");
  if (!commandOverride.empty()) {
    info.command = commandOverride;
  } else {
    // Look up command based on language and package
    std::string key = lang + "_" + info.package;
    auto cmdIt = DefaultCommands.find(key);
    if (cmdIt != DefaultCommands.end()) {
      info.command = cmdIt->second;
    } else {
      // Fallback logic
      if (lang == "CXX") {
        if (info.package == "gcc") {
          info.command = "g++";
        } else if (info.package == "clang") {
          info.command = "clang++";
        } else {
          info.command = info.package + "++"; // Generic C++ suffix
        }
      } else if (lang == "C" || lang == "ASM" || lang == "ASM-ATT") {
        if (info.package == "gcc") {
          info.command = "gcc";
        } else if (info.package == "clang") {
          info.command = "clang";
        } else {
          info.command = info.package; // Use package name as command
        }
      } else {
        // For other languages, command often matches package
        info.command = info.package;
      }
    }
  }
  
  // Set cross-compilation support
  info.supportsCrossCompile = this->SupportsCrossCompilation(lang);
  
  return info;
}

std::string cmNixCompilerResolver::GetCompilerId(const std::string& lang) const
{
  std::string compilerIdVar = "CMAKE_" + lang + "_COMPILER_ID";
  
  cmValue compilerId = this->CMakeInstance->GetState()->GetGlobalProperty(compilerIdVar);
  if (!compilerId) {
    compilerId = this->CMakeInstance->GetCacheDefinition(compilerIdVar);
  }
  
  return compilerId ? *compilerId : "";
}

std::string cmNixCompilerResolver::GetCompilerPath(const std::string& lang) const
{
  std::string compilerVar = "CMAKE_" + lang + "_COMPILER";
  
  cmValue compilerPath = this->CMakeInstance->GetState()->GetGlobalProperty(compilerVar);
  if (!compilerPath) {
    compilerPath = this->CMakeInstance->GetCacheDefinition(compilerVar);
  }
  
  return compilerPath ? *compilerPath : "";
}

std::string cmNixCompilerResolver::GetUserOverride(const std::string& lang, 
                                                    const std::string& varSuffix) const
{
  std::string varName = "CMAKE_NIX_" + lang + varSuffix;
  
  cmValue override = this->CMakeInstance->GetCacheDefinition(varName);
  return override ? *override : "";
}