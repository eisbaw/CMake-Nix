/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */
#pragma once

#include "cmConfigure.h"

#include <string>
#include <vector>
#include <map>

class cmCustomCommand;
class cmLocalGenerator;
class cmGeneratedFileStream;

class cmNixCustomCommandGenerator
{
public:
  cmNixCustomCommandGenerator(cmCustomCommand const* cc, cmLocalGenerator* lg, std::string const& config,
                              const std::map<std::string, std::string>* customCommandOutputs = nullptr);

  void Generate(cmGeneratedFileStream& nixFileStream);
  std::string GetDerivationName() const;
  const std::vector<std::string>& GetOutputs() const;
  const std::vector<std::string>& GetDepends() const;

private:
  // Constants
  static constexpr int HASH_SUFFIX_DIGITS = 10000; // Number of hash digits for unique suffixes
  
  std::string GetDerivationNameForPath(const std::string& path) const;
  
  cmCustomCommand const* CustomCommand;
  cmLocalGenerator* LocalGenerator;
  std::string Config;
  const std::map<std::string, std::string>* CustomCommandOutputs;
};