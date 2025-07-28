/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */
#include "cmNixInstallRuleGenerator.h"

#include <algorithm>
#include <cctype>

#include "cmGeneratorTarget.h"
#include "cmGeneratedFileStream.h"
#include "cmLocalGenerator.h"
#include "cmNixConstants.h"
#include "cmOutputConverter.h"
#include "cmStateTypes.h"
#include "cmTarget.h"
#include "cmInstallGenerator.h"
#include "cmInstallTargetGenerator.h"

cmNixInstallRuleGenerator::cmNixInstallRuleGenerator() = default;
cmNixInstallRuleGenerator::~cmNixInstallRuleGenerator() = default;

std::vector<cmGeneratorTarget*> cmNixInstallRuleGenerator::CollectInstallTargets(
  const std::vector<std::unique_ptr<cmLocalGenerator>>& localGenerators)
{
  std::vector<cmGeneratorTarget*> installTargets;
  
  for (auto const& lg : localGenerators) {
    for (auto const& target : lg->GetGeneratorTargets()) {
      if (target->GetType() == cmStateEnums::EXECUTABLE ||
          target->GetType() == cmStateEnums::STATIC_LIBRARY ||
          target->GetType() == cmStateEnums::SHARED_LIBRARY ||
          target->GetType() == cmStateEnums::MODULE_LIBRARY ||
          target->GetType() == cmStateEnums::OBJECT_LIBRARY) {
        if (!target->Target->GetInstallGenerators().empty()) {
          installTargets.push_back(target.get());
        }
      }
    }
  }
  
  return installTargets;
}

void cmNixInstallRuleGenerator::WriteInstallRules(
  const std::vector<cmGeneratorTarget*>& installTargets,
  cmGeneratedFileStream& nixFileStream,
  const std::string& buildConfiguration,
  std::function<std::string(const std::string&)> getDerivationName)
{
  if (installTargets.empty()) {
    return;
  }
  
  nixFileStream << "\n  # Install derivations\n";
  
  for (cmGeneratorTarget* target : installTargets) {
    std::string targetName = target->GetName();
    std::string derivName = getDerivationName(targetName);
    std::string installDerivName = derivName + "_install";
    
    nixFileStream << "  " << installDerivName << " = stdenv.mkDerivation {\n";
    nixFileStream << "    name = \"" << targetName << "-install\";\n";
    nixFileStream << "    src = " << derivName << ";\n";
    nixFileStream << "    dontUnpack = true;\n";
    nixFileStream << "    dontBuild = true;\n";
    nixFileStream << "    dontConfigure = true;\n";
    nixFileStream << "    installPhase = ''\n";

    // Get install destination, with error handling for missing install generators
    std::string dest;
    const auto& installGens = target->Target->GetInstallGenerators();
    if (installGens.empty()) {
      // No install rules defined - use default destinations
      if (target->GetType() == cmStateEnums::EXECUTABLE) {
        dest = "bin";
      } else if (target->GetType() == cmStateEnums::SHARED_LIBRARY || 
                 target->GetType() == cmStateEnums::STATIC_LIBRARY) {
        dest = "lib";
      } else {
        dest = "share";
      }
    } else {
      dest = installGens[0]->GetDestination(buildConfiguration);
    }
    
    std::string escapedDest = cmOutputConverter::EscapeForShell(dest, cmOutputConverter::Shell_Flag_IsUnix);
    std::string escapedTargetName = cmOutputConverter::EscapeForShell(targetName, cmOutputConverter::Shell_Flag_IsUnix);
    
    nixFileStream << "      mkdir -p $out/" << escapedDest << "\n";
    
    // Determine installation destination based on target type
    if (target->GetType() == cmStateEnums::EXECUTABLE) {
      nixFileStream << "      cp $src $out/" << escapedDest << "/" << escapedTargetName << "\n";
    } else if (target->GetType() == cmStateEnums::SHARED_LIBRARY) {
      nixFileStream << "      cp -r $src/* $out/" << escapedDest << "/ 2>/dev/null || true\n";
    } else if (target->GetType() == cmStateEnums::STATIC_LIBRARY) {
      std::string libName = this->GetLibraryPrefix() + targetName + this->GetStaticLibraryExtension();
      std::string escapedLibName = cmOutputConverter::EscapeForShell(libName, cmOutputConverter::Shell_Flag_IsUnix);
      nixFileStream << "      cp $src $out/" << escapedDest << "/" << escapedLibName << "\n";
    }
    
    nixFileStream << "    '';\n";
    nixFileStream << "  };\n\n";
  }
}

void cmNixInstallRuleGenerator::WriteInstallOutputs(
  const std::vector<cmGeneratorTarget*>& installTargets,
  cmGeneratedFileStream& nixFileStream,
  std::function<std::string(const std::string&)> getDerivationName)
{
  for (cmGeneratorTarget* target : installTargets) {
    std::string targetName = target->GetName();
    std::string derivName = getDerivationName(targetName);
    std::string installDerivName = derivName + "_install";
    
    nixFileStream << "  \"" << targetName << "_install\" = " << installDerivName << ";\n";
  }
}


std::string cmNixInstallRuleGenerator::GetLibraryPrefix() const
{
  // Unix-style library prefix
  return "lib";
}

std::string cmNixInstallRuleGenerator::GetStaticLibraryExtension() const
{
  // Unix-style static library extension
  return cmNix::FilePatterns::STATIC_LIB_SUFFIX;
}