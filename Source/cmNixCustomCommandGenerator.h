/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */
#pragma once

#include "cmConfigure.h"

#include <string>
#include <vector>

class cmCustomCommand;
class cmLocalGenerator;
class cmGeneratedFileStream;

class cmNixCustomCommandGenerator
{
public:
  cmNixCustomCommandGenerator(cmCustomCommand const* cc, cmLocalGenerator* lg, std::string const& config);

  void Generate(cmGeneratedFileStream& nixFileStream);
  std::string GetDerivationName() const;
  std::vector<std::string> GetOutputs() const;

private:
  cmCustomCommand const* CustomCommand;
  cmLocalGenerator* LocalGenerator;
  std::string Config;
};