/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */
#pragma once

#include <iosfwd>
#include <string>
#include <vector>

class cmGeneratedFileStream;

/**
 * \class cmNixWriter
 * \brief Helper class for writing well-formatted Nix expressions
 *
 * This class provides methods to write Nix expressions in a structured way,
 * avoiding string concatenation and improving code readability.
 */
class cmNixWriter
{
public:
  explicit cmNixWriter(cmGeneratedFileStream& stream);

  // Basic writing methods
  void WriteComment(const std::string& comment);
  void WriteLine(const std::string& line = "");
  void WriteIndented(int level, const std::string& line);

  // Nix expression helpers
  void StartDerivation(const std::string& name, int indentLevel = 1);
  void EndDerivation(int indentLevel = 1);
  
  void WriteAttribute(const std::string& name, const std::string& value, 
                     int indentLevel = 2);
  void WriteAttributeBool(const std::string& name, bool value, 
                         int indentLevel = 2);
  void WriteAttributeInt(const std::string& name, int value, 
                        int indentLevel = 2);
  
  // List attributes
  void StartListAttribute(const std::string& name, int indentLevel = 2);
  void WriteListItem(const std::string& item, int indentLevel = 3);
  void EndListAttribute(int indentLevel = 2);
  
  void WriteListAttribute(const std::string& name, 
                         const std::vector<std::string>& items,
                         int indentLevel = 2);
  
  // Multiline strings (for build phases)
  void StartMultilineString(const std::string& name, int indentLevel = 2);
  void WriteMultilineLine(const std::string& line, int indentLevel = 3);
  void EndMultilineString(int indentLevel = 2);
  
  // Source attribute helpers
  void WriteSourceAttribute(const std::string& path, int indentLevel = 2);
  void WriteFilesetUnion(const std::string& name, 
                        const std::vector<std::string>& files,
                        int indentLevel = 2);
  void WriteFilesetUnionSrcAttribute(const std::vector<std::string>& files,
                                    int indentLevel = 2,
                                    const std::string& root = "./.");
  
  // Fileset with maybeMissing for generated files
  void WriteFilesetUnionWithMaybeMissing(const std::vector<std::string>& existingFiles,
                                         const std::vector<std::string>& generatedFiles,
                                         int indentLevel = 2,
                                         const std::string& root = "./.");
  
  // Let binding helpers
  void StartLetBinding(int indentLevel = 0);
  void EndLetBinding(int indentLevel = 0);
  void StartInBlock(int indentLevel = 0);
  
  // Attribute set helpers
  void StartAttributeSet(int indentLevel = 0);
  void EndAttributeSet(int indentLevel = 0);
  
  // Helper for escaping strings
  static std::string EscapeNixString(const std::string& str);
  
  // Helper for making valid Nix identifiers
  static std::string MakeValidNixIdentifier(const std::string& str);

private:
  cmGeneratedFileStream& Stream;
  std::string GetIndent(int level) const;
};