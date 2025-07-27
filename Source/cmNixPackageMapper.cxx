/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */
#include "cmNixPackageMapper.h"

#include <algorithm>
#include <fstream>
#include <sstream>

#include <cm3p/json/json.h>

#include "cmSystemTools.h"

cmNixPackageMapper::cmNixPackageMapper()
{
  this->InitializeMappings();
}

void cmNixPackageMapper::InitializeMappings()
{
  // First try to load from JSON file
  // Try installed location first
  std::string dataRoot = cmSystemTools::GetCMakeRoot();
  std::string jsonPath = dataRoot + "/" + CMAKE_NIX_PACKAGE_MAPPINGS_FILE;
  
  if (!this->LoadMappingsFromFile(jsonPath)) {
    // Try source location for development builds
    jsonPath = dataRoot + "/Source/" + CMAKE_NIX_PACKAGE_MAPPINGS_FILE;
    
    if (!this->LoadMappingsFromFile(jsonPath)) {
      // Fall back to default mappings if file doesn't exist or fails to load
      this->InitializeDefaultMappings();
    }
  }
}

bool cmNixPackageMapper::LoadMappingsFromFile(const std::string& filePath)
{
  if (!cmSystemTools::FileExists(filePath)) {
    return false;
  }
  
  std::ifstream file(filePath);
  if (!file.is_open()) {
    return false;
  }
  
  Json::Value root;
  Json::CharReaderBuilder builder;
  std::string errors;
  
  if (!Json::parseFromStream(builder, file, &root, &errors)) {
    return false;
  }
  
  // Load package mappings
  if (root.isMember("packageMappings") && root["packageMappings"].isObject()) {
    const Json::Value& packageMappings = root["packageMappings"];
    for (const auto& key : packageMappings.getMemberNames()) {
      this->TargetToNixPackage[key] = packageMappings[key].asString();
    }
  }
  
  // Load link flag mappings
  if (root.isMember("linkFlagMappings") && root["linkFlagMappings"].isObject()) {
    const Json::Value& linkFlagMappings = root["linkFlagMappings"];
    for (const auto& key : linkFlagMappings.getMemberNames()) {
      this->TargetToLinkFlags[key] = linkFlagMappings[key].asString();
    }
  }
  
  return true;
}

void cmNixPackageMapper::InitializeDefaultMappings()
{
  // Default mappings as fallback
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
    
    // XML/JSON/YAML
    {"LibXml2::LibXml2", "libxml2"},
    {"RapidJSON::RapidJSON", "rapidjson"},
    {"yaml-cpp", "libyaml-cpp"},
    
    // Image processing
    {"OpenCV::OpenCV", "opencv"},
    {"ImageMagick::ImageMagick", "imagemagick"},
    
    // Compression
    {"BZip2::BZip2", "bzip2"},
    {"LibLZMA::LibLZMA", "xz"},
    {"ZSTD::ZSTD", "zstd"},
    
    // Cryptography
    {"GnuTLS::GnuTLS", "gnutls"},
    {"LibGcrypt::LibGcrypt", "libgcrypt"},
    
    // Audio
    {"ALSA::ALSA", "alsa-lib"},
    {"PulseAudio::PulseAudio", "libpulseaudio"},
    
    // GUI
    {"Qt5::Core", "qt5.qtbase"},
    {"Qt6::Core", "qt6.qtbase"},
    {"GTK3::GTK3", "gtk3"},
    {"wxWidgets::wxWidgets", "wxGTK32"},
    
    // Python
    {"Python3::Python", "python3"},
    {"Python3::NumPy", "python3Packages.numpy"},
    
    // Other languages
    {"Ruby::Ruby", "ruby"},
    {"Lua::Lua", "lua"},
    
    // Scientific computing
    {"BLAS::BLAS", "blas"},
    {"LAPACK::LAPACK", "lapack"},
    {"HDF5::HDF5", "hdf5"},
    
    // Messaging/IPC
    {"ZeroMQ::ZeroMQ", "zeromq"},
    {"RabbitMQ::RabbitMQ", "rabbitmq-c"},
    
    // Testing
    {"GTest::GTest", "gtest"},
    {"GTest::Main", "gtest"},
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
    {"LibXml2::LibXml2", "-lxml2"},
    {"yaml-cpp", "-lyaml-cpp"},
    {"BZip2::BZip2", "-lbz2"},
    {"LibLZMA::LibLZMA", "-llzma"},
    {"ZSTD::ZSTD", "-lzstd"},
    {"GnuTLS::GnuTLS", "-lgnutls"},
    {"LibGcrypt::LibGcrypt", "-lgcrypt"},
    {"ALSA::ALSA", "-lasound"},
    {"PulseAudio::PulseAudio", "-lpulse"},
    {"HDF5::HDF5", "-lhdf5"},
    {"ZeroMQ::ZeroMQ", "-lzmq"},
    {"RabbitMQ::RabbitMQ", "-lrabbitmq"},
    {"GTest::GTest", "-lgtest"},
    {"GTest::Main", "-lgtest_main"},
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