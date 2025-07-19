/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */
#include "cmNixPackageMapper.h"

#include <map>
#include <string>

// Map CMake package names to Nix packages
const std::map<std::string, std::string> cmNixPackageMapper::PackageMap = {
  {"Threads", ""},        // Built into compiler, no extra package needed
  {"ZLIB", "zlib"},       
  {"OpenGL", "libGL"},    
  {"GLUT", "freeglut"},   
  {"X11", "xorg.libX11"}, 
  {"PNG", "libpng"},      
  {"JPEG", "libjpeg"},    
  {"CURL", "curl"},       
  {"OpenSSL", "openssl"}, 
  {"Boost", "boost"},     
  {"Qt5", "qt5"},         
  {"GTK3", "gtk3"},       
  {"SDL2", "SDL2"},
  {"PkgConfig", "pkg-config"}
};

// Map imported targets to Nix packages  
const std::map<std::string, std::string> cmNixPackageMapper::TargetMap = {
  {"Threads::Threads", ""},           // Built into compiler
  {"ZLIB::ZLIB", "zlib"},
  {"OpenGL::GL", "libGL"},
  {"OpenGL::GLU", "libGLU"},
  {"GLUT::GLUT", "freeglut"},
  {"X11::X11", "xorg.libX11"},
  {"PNG::PNG", "libpng"},
  {"JPEG::JPEG", "libjpeg"},
  {"CURL::libcurl", "curl"},
  {"OpenSSL::SSL", "openssl"},
  {"OpenSSL::Crypto", "openssl"},
  {"PkgConfig::pkgconf", "pkg-config"}
};

// Map imported targets to required link flags
const std::map<std::string, std::string> cmNixPackageMapper::LinkFlagsMap = {
  {"Threads::Threads", "-lpthread"},
  {"ZLIB::ZLIB", "-lz"},
  {"OpenGL::GL", "-lGL"},
  {"OpenGL::GLU", "-lGLU"},
  {"GLUT::GLUT", "-lglut"},
  {"X11::X11", "-lX11"},
  {"PNG::PNG", "-lpng"},
  {"JPEG::JPEG", "-ljpeg"},
  {"CURL::libcurl", "-lcurl"},
  {"OpenSSL::SSL", "-lssl"},
  {"OpenSSL::Crypto", "-lcrypto"},
  {"PkgConfig::pkgconf", ""}
};

std::string cmNixPackageMapper::GetNixPackage(const std::string& cmakePackage) const
{
  auto it = PackageMap.find(cmakePackage);
  return (it != PackageMap.end()) ? it->second : "";
}

std::string cmNixPackageMapper::GetNixPackageForTarget(const std::string& importedTarget) const
{
  auto it = TargetMap.find(importedTarget);
  return (it != TargetMap.end()) ? it->second : "";
}

std::string cmNixPackageMapper::GetLinkFlags(const std::string& importedTarget) const
{
  auto it = LinkFlagsMap.find(importedTarget);
  return (it != LinkFlagsMap.end()) ? it->second : "";
}