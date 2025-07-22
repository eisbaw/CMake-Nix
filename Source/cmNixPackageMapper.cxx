/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */
#include "cmNixPackageMapper.h"

#include <algorithm>

cmNixPackageMapper::cmNixPackageMapper()
{
  this->InitializeMappings();
}

void cmNixPackageMapper::InitializeMappings()
{
  // Map common CMake imported targets to Nix packages
  this->TargetToNixPackage = {
    // Built into compiler - no package needed
    {"Threads::Threads", ""},
    
    // OpenGL
    {"OpenGL::GL", "libGL"},
    {"OpenGL::GLU", "libGLU"},
    {"OpenGL::GLEW", "glew"},
    {"GLFW", "glfw"},
    
    // Math and system libraries
    {"m", "glibc"},
    {"pthread", "glibc"},
    {"dl", "glibc"},
    {"rt", "glibc"},
    
    // Common development libraries
    {"ZLIB::ZLIB", "zlib"},
    {"PNG::PNG", "libpng"},
    {"JPEG::JPEG", "libjpeg"},
    {"OpenSSL::SSL", "openssl"},
    {"OpenSSL::Crypto", "openssl"},
    
    // Audio/Video
    {"SDL2::SDL2", "SDL2"},
    {"SDL2_image::SDL2_image", "SDL2_image"},
    {"SDL2_mixer::SDL2_mixer", "SDL2_mixer"},
    {"SDL2_ttf::SDL2_ttf", "SDL2_ttf"},
    
    // Network
    {"CURL::libcurl", "curl"},
    
    // Database
    {"SQLite::SQLite3", "sqlite"},
    
    // Development tools
    {"Boost::boost", "boost"},
    {"Protobuf::Protobuf", "protobuf"},
  };

  this->TargetToLinkFlags = {
    {"Threads::Threads", "-lpthread"},
    {"OpenGL::GL", "-lGL"},
    {"OpenGL::GLU", "-lGLU"},
    {"OpenGL::GLEW", "-lGLEW"},
    {"GLFW", "-lglfw"},
    {"m", "-lm"},
    {"pthread", "-lpthread"},
    {"dl", "-ldl"},
    {"rt", "-lrt"},
    {"ZLIB::ZLIB", "-lz"},
    {"PNG::PNG", "-lpng"},
    {"JPEG::JPEG", "-ljpeg"},
    {"OpenSSL::SSL", "-lssl"},
    {"OpenSSL::Crypto", "-lcrypto"},
    {"SDL2::SDL2", "-lSDL2"},
    {"SDL2_image::SDL2_image", "-lSDL2_image"},
    {"SDL2_mixer::SDL2_mixer", "-lSDL2_mixer"},
    {"SDL2_ttf::SDL2_ttf", "-lSDL2_ttf"},
    {"CURL::libcurl", "-lcurl"},
    {"SQLite::SQLite3", "-lsqlite3"},
  };
}

std::string cmNixPackageMapper::GetNixPackageForTarget(std::string const& targetName) const
{
  auto it = this->TargetToNixPackage.find(targetName);
  if (it != this->TargetToNixPackage.end()) {
    return it->second;
  }
  
  // Try some heuristics for common patterns
  if (targetName.find("::") != std::string::npos) {
    // CMake imported target format (e.g., "MyLib::MyLib")
    size_t pos = targetName.find("::");
    std::string baseName = targetName.substr(0, pos);
    std::transform(baseName.begin(), baseName.end(), baseName.begin(), ::tolower);
    return baseName;
  }
  
  // Default: assume library name maps directly to Nix package name
  std::string mapped = targetName;
  std::transform(mapped.begin(), mapped.end(), mapped.begin(), ::tolower);
  return mapped;
}

std::string cmNixPackageMapper::GetLinkFlags(std::string const& targetName) const
{
  auto it = this->TargetToLinkFlags.find(targetName);
  if (it != this->TargetToLinkFlags.end()) {
    return it->second;
  }
  return "";
}
