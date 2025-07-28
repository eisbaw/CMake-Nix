/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */
#include "cmNixDerivationWriter.h"

#include <sstream>
#include <iostream>

#include "cmGeneratedFileStream.h"
#include "cmGeneratorTarget.h"
#include "cmSourceFile.h"
#include "cmNixWriter.h"
#include "cmStateTypes.h"
#include "cmSystemTools.h"
#include "cmNixPathUtils.h"
#include "cmStringAlgorithms.h"
#include "cmLocalGenerator.h"

cmNixDerivationWriter::cmNixDerivationWriter() = default;

cmNixDerivationWriter::~cmNixDerivationWriter() = default;

void cmNixDerivationWriter::LogDebug(const std::string& message) const
{
  if (this->DebugOutput) {
    std::cerr << "[NIX-DEBUG] " << message << std::endl;
  }
}

void cmNixDerivationWriter::WriteObjectDerivationWithHelper(
  cmGeneratedFileStream& nixFileStream,
  const std::string& derivName,
  const std::string& objectName,
  const std::string& srcPath,
  const std::string& sourcePath,
  const std::string& compilerPackage,
  const std::string& compileFlags,
  const std::vector<std::string>& buildInputs)
{
  // Start the derivation using cmakeNixCC helper
  nixFileStream << "  " << derivName << " = cmakeNixCC {\n";
  nixFileStream << "    name = \"" << objectName << "\";\n";
  nixFileStream << "    src = " << srcPath << ";\n";
  
  // Write source path
  if (sourcePath.find("${") != std::string::npos) {
    nixFileStream << "    source = \"" << sourcePath << "\";\n";
  } else {
    nixFileStream << "    source = \"" << cmNixWriter::EscapeNixString(sourcePath) << "\";\n";
  }
  
  // Write compiler
  nixFileStream << "    compiler = " << compilerPackage << ";\n";
  
  // Write flags
  if (!compileFlags.empty()) {
    nixFileStream << "    flags = \"" << cmNixWriter::EscapeNixString(compileFlags) << "\";\n";
  }
  
  // Write build inputs
  if (!buildInputs.empty()) {
    nixFileStream << "    buildInputs = [ ";
    bool first = true;
    for (const auto& input : buildInputs) {
      if (!first) {
        nixFileStream << " ";
      }
      nixFileStream << input;
      first = false;
    }
    nixFileStream << " ];\n";
  }
  
  nixFileStream << "  };\n\n";
}

void cmNixDerivationWriter::WriteLinkDerivation(
  cmGeneratedFileStream& nixFileStream,
  cmGeneratorTarget* target,
  const std::string& derivName,
  const std::string& outputName,
  const std::string& targetType,
  const std::vector<std::string>& buildInputs,
  const std::vector<std::string>& objects,
  const std::string& linkCommand,
  const std::string& config)
{
  nixFileStream << "  " << derivName << " = stdenv.mkDerivation {\n";
  nixFileStream << "    name = \"" << outputName << "\";\n";
  
  // Write build inputs
  nixFileStream << "    buildInputs = [ ";
  bool first = true;
  for (const auto& input : buildInputs) {
    if (!first) {
      nixFileStream << " ";
    }
    nixFileStream << input;
    first = false;
  }
  nixFileStream << " ];\n";
  
  // Write objects as nativeBuildInputs for proper dependency tracking
  if (!objects.empty()) {
    nixFileStream << "    objects = [ ";
    first = true;
    for (const auto& obj : objects) {
      if (!first) {
        nixFileStream << " ";
      }
      nixFileStream << obj;
      first = false;
    }
    nixFileStream << " ];\n";
  }
  
  // Write phases
  nixFileStream << "    phases = [ \"buildPhase\" ];\n";
  
  // Write build phase
  nixFileStream << "    buildPhase = ''\n";
  nixFileStream << "      runHook preBuild\n";
  
  // Create output directory for libraries
  if (targetType == "SHARED_LIBRARY" || targetType == "STATIC_LIBRARY" || 
      targetType == "MODULE_LIBRARY") {
    nixFileStream << "      mkdir -p $out\n";
  } else {
    nixFileStream << "      mkdir -p \"$(dirname \"$out\")\"\n";
  }
  
  // Write the link command
  nixFileStream << "      " << linkCommand << "\n";
  
  nixFileStream << "      runHook postBuild\n";
  nixFileStream << "    '';\n";
  nixFileStream << "  };\n\n";
}

void cmNixDerivationWriter::WriteCustomCommandDerivation(
  cmGeneratedFileStream& nixFileStream,
  const std::string& derivName,
  const std::vector<std::string>& outputs,
  const std::vector<std::string>& depends,
  const std::vector<std::string>& commands,
  const std::string& workingDir,
  const std::string& srcPath)
{
  nixFileStream << "  " << derivName << " = stdenv.mkDerivation {\n";
  nixFileStream << "    name = \"" << derivName << "\";\n";
  nixFileStream << "    src = " << srcPath << ";\n";
  
  // Write dependencies
  if (!depends.empty()) {
    nixFileStream << "    buildInputs = [ ";
    bool first = true;
    for (const auto& dep : depends) {
      if (!first) {
        nixFileStream << " ";
      }
      nixFileStream << dep;
      first = false;
    }
    nixFileStream << " ];\n";
  }
  
  // Write outputs
  if (outputs.size() > 1) {
    nixFileStream << "    outputs = [ \"out\" ";
    for (size_t i = 1; i < outputs.size(); ++i) {
      nixFileStream << " \"output" << i << "\"";
    }
    nixFileStream << " ];\n";
  }
  
  nixFileStream << "    phases = [ \"unpackPhase\" \"buildPhase\" ];\n";
  nixFileStream << "    buildPhase = ''\n";
  nixFileStream << "      runHook preBuild\n";
  
  // Change to working directory if specified
  if (!workingDir.empty() && workingDir != ".") {
    nixFileStream << "      cd \"" << workingDir << "\"\n";
  }
  
  // Execute commands
  for (const auto& command : commands) {
    nixFileStream << "      " << command << "\n";
  }
  
  // Handle multiple outputs
  if (outputs.size() > 1) {
    for (size_t i = 1; i < outputs.size(); ++i) {
      nixFileStream << "      cp \"" << outputs[i] << "\" \"$output" << i << "\"\n";
    }
  }
  
  nixFileStream << "      runHook postBuild\n";
  nixFileStream << "    '';\n";
  nixFileStream << "  };\n\n";
}

void cmNixDerivationWriter::WriteNixHelperFunctions(cmNixWriter& writer)
{
  writer.WriteComment("Helper function to create a fileset union from a list of paths");
  writer.WriteLine("  makeFilesetUnion = rootPath: paths:");
  writer.WriteLine("    let");
  writer.WriteLine("      # Convert a path to a fileset, handling both files and directories");
  writer.WriteLine("      toFileset = path:");
  writer.WriteLine("        if builtins.pathExists path then");
  writer.WriteLine("          if lib.pathIsDirectory path then");
  writer.WriteLine("            lib.fileset.fromSource (lib.sources.sourceByRegex rootPath [\"${path}/.*\"])");
  writer.WriteLine("          else");
  writer.WriteLine("            lib.fileset.fromSource (lib.sources.sourceByRegex rootPath [\"${path}\"])");
  writer.WriteLine("        else");
  writer.WriteLine("          lib.fileset.fromSource (lib.sources.sourceByRegex rootPath []);");
  writer.WriteLine("          ");
  writer.WriteLine("      # Create filesets for all paths");
  writer.WriteLine("      filesets = map toFileset paths;");
  writer.WriteLine("      ");
  writer.WriteLine("      # Start with an empty fileset");
  writer.WriteLine("      emptySet = lib.fileset.fromSource (lib.sources.sourceByRegex rootPath []);");
  writer.WriteLine("    in");
  writer.WriteLine("      # Union all filesets together");
  writer.WriteLine("      builtins.foldl' lib.fileset.union emptySet filesets;");
  writer.WriteLine();
  
  // Add debug comment to verify this method is being called
  writer.WriteComment("Helper functions will be moved to cmNixDerivationWriter");
}

void cmNixDerivationWriter::WriteFilesetUnion(
  cmGeneratedFileStream& nixFileStream,
  const std::vector<std::string>& existingFiles,
  const std::vector<std::string>& generatedFiles,
  const std::string& rootPath)
{
  if (existingFiles.empty() && generatedFiles.empty()) {
    nixFileStream << "./.";
    return;
  }
  
  nixFileStream << "lib.fileset.toSource {\n";
  nixFileStream << "      root = " << rootPath << ";\n";
  nixFileStream << "      fileset = lib.fileset.unions [\n";
  
  // Add existing files
  for (const auto& file : existingFiles) {
    nixFileStream << "        " << file << "\n";
  }
  
  // Add generated files
  for (const auto& file : generatedFiles) {
    nixFileStream << "        " << file << "\n";
  }
  
  nixFileStream << "      ];\n";
  nixFileStream << "    }";
}

void cmNixDerivationWriter::WriteCompositeSource(
  cmGeneratedFileStream& nixFileStream,
  const std::vector<std::string>& configTimeGeneratedFiles,
  const std::string& srcDir,
  const std::string& buildDir,
  cmGeneratorTarget* target,
  const std::string& lang,
  const std::string& config,
  const std::vector<std::string>& customCommandHeaders)
{
  // Check if we have any configuration-time generated files
  bool hasConfigTimeFiles = !configTimeGeneratedFiles.empty();
  
  if (!hasConfigTimeFiles && customCommandHeaders.empty()) {
    // No special handling needed
    return;
  }
  
  nixFileStream << "stdenv.mkDerivation {\n";
  nixFileStream << "      name = \"composite-src-with-generated\";\n";
  nixFileStream << "      phases = [ \"installPhase\" ];\n";
  
  // Add custom command headers as build inputs if any
  if (!customCommandHeaders.empty()) {
    nixFileStream << "      buildInputs = [ ";
    bool first = true;
    for (const auto& header : customCommandHeaders) {
      if (!first) {
        nixFileStream << " ";
      }
      nixFileStream << header;
      first = false;
    }
    nixFileStream << " ];\n";
  }
  
  nixFileStream << "      installPhase = ''\n";
  nixFileStream << "        mkdir -p $out\n";
  nixFileStream << "        cp -r " << srcDir << "/* $out/ || true\n";
  
  // Copy configuration-time generated files
  if (hasConfigTimeFiles) {
    for (const auto& file : configTimeGeneratedFiles) {
      std::string relPath = cmSystemTools::RelativePath(buildDir, file);
      std::string destDir = cmSystemTools::GetFilenamePath(relPath);
      
      nixFileStream << "        mkdir -p $out/" << destDir << "\n";
      nixFileStream << "        cat > $out/" << relPath << " <<'EOF'\n";
      
      // Read and embed the file content
      std::string content;
      if (cmSystemTools::FileExists(file) && !cmSystemTools::FileIsDirectory(file)) {
        cmsys::ifstream fin(file.c_str());
        if (fin) {
          std::string line;
          while (std::getline(fin, line)) {
            nixFileStream << line << "\n";
          }
        }
      }
      
      nixFileStream << "EOF\n";
    }
  }
  
  // Copy custom command outputs
  if (!customCommandHeaders.empty()) {
    nixFileStream << "        # Copy custom command outputs\n";
    nixFileStream << "        for input in $buildInputs; do\n";
    nixFileStream << "          if [ -f \"$input\" ]; then\n";
    nixFileStream << "            cp \"$input\" \"$out/$(basename \"$input\")\"\n";
    nixFileStream << "          elif [ -d \"$input\" ]; then\n";
    nixFileStream << "            cp -r \"$input\"/* \"$out/\" || true\n";
    nixFileStream << "          fi\n";
    nixFileStream << "        done\n";
  }
  
  nixFileStream << "      '';\n";
  nixFileStream << "    }";
}

void cmNixDerivationWriter::WriteExternalHeaderDerivation(
  cmGeneratedFileStream& nixFileStream,
  const std::string& derivName,
  const std::vector<std::string>& headers,
  const std::string& sourceDir)
{
  nixFileStream << "  " << derivName << " = stdenv.mkDerivation {\n";
  nixFileStream << "    name = \"" << derivName << "\";\n";
  nixFileStream << "    src = " << sourceDir << ";\n";
  nixFileStream << "    phases = [ \"installPhase\" ];\n";
  nixFileStream << "    installPhase = ''\n";
  nixFileStream << "      mkdir -p $out\n";
  
  // Copy each header file, preserving directory structure
  for (const auto& header : headers) {
    std::string relPath = cmNixPathUtils::MakeProjectRelative(header, sourceDir);
    std::string destDir = cmSystemTools::GetFilenamePath(relPath);
    
    if (!destDir.empty()) {
      nixFileStream << "      mkdir -p $out/" << destDir << "\n";
    }
    nixFileStream << "      cp " << header << " $out/" << relPath << "\n";
  }
  
  nixFileStream << "    '';\n";
  nixFileStream << "  };\n\n";
}

void cmNixDerivationWriter::WriteInstallDerivation(
  cmGeneratedFileStream& nixFileStream,
  const std::string& derivName,
  const std::string& sourcePath,
  const std::string& installCommand)
{
  nixFileStream << "  " << derivName << " = stdenv.mkDerivation {\n";
  nixFileStream << "    name = \"" << derivName << "\";\n";
  nixFileStream << "    src = " << sourcePath << ";\n";
  nixFileStream << "    phases = [ \"installPhase\" ];\n";
  nixFileStream << "    installPhase = ''\n";
  nixFileStream << "      runHook preInstall\n";
  nixFileStream << "      " << installCommand << "\n";
  nixFileStream << "      runHook postInstall\n";
  nixFileStream << "    '';\n";
  nixFileStream << "  };\n\n";
}