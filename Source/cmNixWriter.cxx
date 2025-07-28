/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */
#include "cmNixWriter.h"

#include <algorithm>
#include <sstream>

#include "cmGeneratedFileStream.h"

// Constants for code formatting
/**
 * Number of spaces per indentation level in generated Nix code.
 * Nix community standard is 2 spaces per indent level.
 */
static constexpr int SPACES_PER_INDENT = 2;

/**
 * Extra space to reserve when escaping strings.
 * This accounts for escape characters that might be added.
 * A reserve of 10 characters handles most common escape sequences
 * without requiring reallocation.
 */
static constexpr size_t STRING_ESCAPE_RESERVE = 10;

cmNixWriter::cmNixWriter(cmGeneratedFileStream& stream)
  : Stream(stream)
{
}

void cmNixWriter::WriteComment(const std::string& comment)
{
  Stream << "# " << comment << "\n";
}

void cmNixWriter::WriteLine(const std::string& line)
{
  Stream << line << "\n";
}

void cmNixWriter::WriteIndented(int level, const std::string& line)
{
  Stream << GetIndent(level) << line << "\n";
}

void cmNixWriter::StartDerivation(const std::string& name, int indentLevel)
{
  WriteIndented(indentLevel, name + " = stdenv.mkDerivation {");
}

void cmNixWriter::EndDerivation(int indentLevel)
{
  WriteIndented(indentLevel, "};");
}

void cmNixWriter::WriteAttribute(const std::string& name, 
                                const std::string& value, int indentLevel)
{
  WriteIndented(indentLevel, name + " = \"" + EscapeNixString(value) + "\";");
}

void cmNixWriter::WriteAttributeBool(const std::string& name, bool value, 
                                    int indentLevel)
{
  WriteIndented(indentLevel, name + " = " + (value ? "true" : "false") + ";");
}

void cmNixWriter::WriteAttributeInt(const std::string& name, int value, 
                                   int indentLevel)
{
  WriteIndented(indentLevel, name + " = " + std::to_string(value) + ";");
}

void cmNixWriter::StartListAttribute(const std::string& name, int indentLevel)
{
  WriteIndented(indentLevel, name + " = [");
}

void cmNixWriter::WriteListItem(const std::string& item, int indentLevel)
{
  WriteIndented(indentLevel, item);
}

void cmNixWriter::EndListAttribute(int indentLevel)
{
  WriteIndented(indentLevel, "];");
}

void cmNixWriter::WriteListAttribute(const std::string& name, 
                                    const std::vector<std::string>& items,
                                    int indentLevel)
{
  if (items.empty()) {
    WriteIndented(indentLevel, name + " = [ ];");
    return;
  }
  
  if (items.size() == 1) {
    WriteIndented(indentLevel, name + " = [ " + items[0] + " ];");
    return;
  }
  
  StartListAttribute(name, indentLevel);
  for (const auto& item : items) {
    WriteListItem(item, indentLevel + 1);
  }
  EndListAttribute(indentLevel);
}

void cmNixWriter::StartMultilineString(const std::string& name, int indentLevel)
{
  WriteIndented(indentLevel, name + " = ''");
}

void cmNixWriter::WriteMultilineLine(const std::string& line, int indentLevel)
{
  WriteIndented(indentLevel, line);
}

void cmNixWriter::EndMultilineString(int indentLevel)
{
  WriteIndented(indentLevel, "'';");
}

void cmNixWriter::WriteSourceAttribute(const std::string& path, int indentLevel)
{
  if (path == "./.") {
    WriteIndented(indentLevel, "src = ./.;");
  } else {
    WriteIndented(indentLevel, "src = " + path + ";");
  }
}

void cmNixWriter::WriteFilesetUnion(const std::string& name, 
                                   const std::vector<std::string>& files,
                                   int indentLevel)
{
  if (files.empty()) {
    WriteIndented(indentLevel, name + " = ./.;");
    return;
  }
  
  if (files.size() == 1) {
    WriteIndented(indentLevel, name + " = ./" + files[0] + ";");
    return;
  }
  
  // Use Nix fileset union for multiple files
  WriteIndented(indentLevel, name + " = lib.fileset.unions [");
  for (const auto& file : files) {
    WriteIndented(indentLevel + 1, "./" + file);
  }
  WriteIndented(indentLevel, "];");
}

void cmNixWriter::WriteFilesetUnionSrcAttribute(const std::vector<std::string>& files,
                                               int indentLevel,
                                               const std::string& root)
{
  if (files.empty()) {
    WriteIndented(indentLevel, "src = " + root + ";");
    return;
  }
  
  // For single files, we need to use fileset.toSource
  if (files.size() == 1) {
    WriteIndented(indentLevel, "src = lib.fileset.toSource {");
    WriteIndented(indentLevel + 1, "root = " + root + ";");
    // Don't add extra slash if root already ends with one
    std::string separator = (root.back() == '/') ? "" : "/";
    WriteIndented(indentLevel + 1, "fileset = " + root + separator + files[0] + ";");
    WriteIndented(indentLevel, "};");
    return;
  }
  
  // For multiple files, use fileset union with toSource
  WriteIndented(indentLevel, "src = lib.fileset.toSource {");
  WriteIndented(indentLevel + 1, "root = " + root + ";");
  WriteIndented(indentLevel + 1, "fileset = lib.fileset.unions [");
  for (const auto& file : files) {
    // Don't add extra slash if root already ends with one
    std::string separator = (root.back() == '/') ? "" : "/";
    WriteIndented(indentLevel + 2, root + separator + file);
  }
  WriteIndented(indentLevel + 1, "];");
  WriteIndented(indentLevel, "};");
}

void cmNixWriter::WriteFilesetUnionWithMaybeMissing(
    const std::vector<std::string>& existingFiles,
    const std::vector<std::string>& generatedFiles,
    int indentLevel,
    const std::string& root)
{
  // If no files at all, just use root
  if (existingFiles.empty() && generatedFiles.empty()) {
    WriteIndented(indentLevel, "src = " + root + ";");
    return;
  }
  
  WriteIndented(indentLevel, "src = lib.fileset.toSource {");
  WriteIndented(indentLevel + 1, "root = " + root + ";");
  
  std::string separator = (root.back() == '/') ? "" : "/";
  
  // Handle single file case
  if (existingFiles.size() + generatedFiles.size() == 1) {
    if (!existingFiles.empty()) {
      WriteIndented(indentLevel + 1, "fileset = " + root + separator + existingFiles[0] + ";");
    } else {
      WriteIndented(indentLevel + 1, "fileset = lib.fileset.maybeMissing (" + root + separator + generatedFiles[0] + ");");
    }
  } else {
    // Multiple files - use union
    WriteIndented(indentLevel + 1, "fileset = lib.fileset.unions [");
    
    // Add existing files
    for (const auto& file : existingFiles) {
      WriteIndented(indentLevel + 2, root + separator + file);
    }
    
    // Add generated files with maybeMissing
    for (const auto& file : generatedFiles) {
      WriteIndented(indentLevel + 2, "(lib.fileset.maybeMissing (" + root + separator + file + "))");
    }
    
    WriteIndented(indentLevel + 1, "];");
  }
  
  WriteIndented(indentLevel, "};");
}

void cmNixWriter::StartLetBinding(int indentLevel)
{
  WriteIndented(indentLevel, "let");
}

void cmNixWriter::EndLetBinding(int indentLevel)
{
  WriteIndented(indentLevel, "in");
}

void cmNixWriter::StartInBlock(int indentLevel)
{
  // The "in" was already written by EndLetBinding
  StartAttributeSet(indentLevel);
}

void cmNixWriter::StartAttributeSet(int indentLevel)
{
  WriteIndented(indentLevel, "{");
}

void cmNixWriter::EndAttributeSet(int indentLevel)
{
  WriteIndented(indentLevel, "}");
}

std::string cmNixWriter::GetIndent(int level) const
{
  return std::string(level * SPACES_PER_INDENT, ' ');
}

std::string cmNixWriter::EscapeNixString(const std::string& str)
{
  std::string result;
  result.reserve(str.size() + STRING_ESCAPE_RESERVE);
  
  for (char c : str) {
    switch (c) {
      case '"':
        result += "\\\"";
        break;
      case '\\':
        result += "\\\\";
        break;
      case '\n':
        result += "\\n";
        break;
      case '\r':
        result += "\\r";
        break;
      case '\t':
        result += "\\t";
        break;
      case '$':
        result += "\\$";
        break;
      case '`':
        result += "\\`";
        break;
      default:
        result += c;
        break;
    }
  }
  
  return result;
}

std::string cmNixWriter::MakeValidNixIdentifier(const std::string& str)
{
  std::string result;
  result.reserve(str.size());
  
  // Replace invalid characters with underscores
  for (char c : str) {
    if ((c >= 'a' && c <= 'z') || 
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') ||
        c == '_' || c == '-') {
      result += c;
    } else {
      result += '_';
    }
  }
  
  // Ensure it doesn't start with a digit
  if (!result.empty() && result[0] >= '0' && result[0] <= '9') {
    result = "_" + result;
  }
  
  // Handle reserved words (const static is thread-safe)
  static const std::vector<std::string> reservedWords = {
    "let", "in", "if", "then", "else", "assert", "with", "rec", "inherit"
  };
  
  if (std::find(reservedWords.begin(), reservedWords.end(), result) != 
      reservedWords.end()) {
    result = "_" + result;
  }
  
  return result;
}