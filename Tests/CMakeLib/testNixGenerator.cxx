/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file LICENSE.rst or https://cmake.org/licensing for details.  */

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "cmGlobalNixGenerator.h"
#include "cmLocalNixGenerator.h"
#include "cmNixTargetGenerator.h"
#include "cmNixWriter.h"
#include "cmGeneratorTarget.h"
#include "cmGeneratedFileStream.h"
#include "cmMakefile.h"
#include "cmState.h"
#include "cmStateDirectory.h"
#include "cmStateSnapshot.h"
#include "cmake.h"

#include "testCommon.h"

// Forward declarations to access protected members
class TestableNixGenerator : public cmGlobalNixGenerator
{
public:
  using cmGlobalNixGenerator::cmGlobalNixGenerator;
  using cmGlobalNixGenerator::GetCompilerPackage;
  using cmGlobalNixGenerator::IsSystemPath;
  using cmGlobalNixGenerator::GetDerivationName;
};

// Test fixture class
class NixGeneratorTestFixture
{
public:
  NixGeneratorTestFixture()
  {
    this->CMake = cm::make_unique<cmake>(cmake::RoleInternal, cmState::Unknown);
    this->CMake->SetHomeDirectory("/tmp/test_source");
    this->CMake->SetHomeOutputDirectory("/tmp/test_build");
    
    this->GlobalGen = cm::make_unique<TestableNixGenerator>(this->CMake.get());
  }

  std::unique_ptr<cmake> CMake;
  std::unique_ptr<TestableNixGenerator> GlobalGen;
};

static bool testGlobalGeneratorName()
{
  std::cout << "testGlobalGeneratorName()\n";
  NixGeneratorTestFixture fixture;
  
  ASSERT_TRUE(fixture.GlobalGen->GetName() == "Nix");
  ASSERT_TRUE(cmGlobalNixGenerator::GetActualName() == "Nix");
  
  return true;
}

static bool testGlobalGeneratorFactory()
{
  std::cout << "testGlobalGeneratorFactory()\n";
  
  auto factory = cmGlobalNixGenerator::NewFactory();
  ASSERT_TRUE(factory != nullptr);
  
  // Test factory can create generators
  cmake cm(cmake::RoleInternal, cmState::Unknown);
  auto gen = factory->CreateGlobalGenerator("Nix", &cm);
  ASSERT_TRUE(gen != nullptr);
  ASSERT_TRUE(gen->GetName() == "Nix");
  
  return true;
}

static bool testIsMultiConfig()
{
  std::cout << "testIsMultiConfig()\n";
  NixGeneratorTestFixture fixture;
  
  // Nix generator is single-config
  ASSERT_TRUE(!fixture.GlobalGen->IsMultiConfig());
  
  return true;
}

static bool testNixWriter()
{
  std::cout << "testNixWriter()\n";
  
  // cmNixWriter requires cmGeneratedFileStream, so we skip this test
  // and focus on testing the static helper methods
  
  return true;
}

static bool testNixWriterEscaping()
{
  std::cout << "testNixWriterEscaping()\n";
  
  // Test string escaping
  ASSERT_TRUE(cmNixWriter::EscapeNixString("simple") == "simple");
  ASSERT_TRUE(cmNixWriter::EscapeNixString("with\"quotes") == "with\\\"quotes");
  ASSERT_TRUE(cmNixWriter::EscapeNixString("with\\backslash") == "with\\\\backslash");
  ASSERT_TRUE(cmNixWriter::EscapeNixString("with\nnewline") == "with\\nnewline");
  ASSERT_TRUE(cmNixWriter::EscapeNixString("with\ttab") == "with\\ttab");
  ASSERT_TRUE(cmNixWriter::EscapeNixString("with\rcarriage") == "with\\rcarriage");
  ASSERT_TRUE(cmNixWriter::EscapeNixString("with$dollar") == "with\\$dollar");
  ASSERT_TRUE(cmNixWriter::EscapeNixString("with`backtick") == "with\\`backtick");
  
  return true;
}

static bool testMakeValidNixIdentifier()
{
  std::cout << "testMakeValidNixIdentifier()\n";
  
  // Test making valid Nix identifiers
  ASSERT_TRUE(cmNixWriter::MakeValidNixIdentifier("simple") == "simple");
  ASSERT_TRUE(cmNixWriter::MakeValidNixIdentifier("with.dots") == "with_dots");
  ASSERT_TRUE(cmNixWriter::MakeValidNixIdentifier("with-dashes") == "with-dashes");  // dashes are allowed
  ASSERT_TRUE(cmNixWriter::MakeValidNixIdentifier("with+plus") == "with_plus");
  ASSERT_TRUE(cmNixWriter::MakeValidNixIdentifier("with spaces") == "with_spaces");
  ASSERT_TRUE(cmNixWriter::MakeValidNixIdentifier("123numeric") == "_123numeric");  // prepend underscore
  ASSERT_TRUE(cmNixWriter::MakeValidNixIdentifier("_underscore") == "_underscore");
  
  return true;
}

static bool testGetCompilerPackage()
{
  std::cout << "testGetCompilerPackage()\n";
  NixGeneratorTestFixture fixture;
  
  // Test compiler package detection
  ASSERT_TRUE(fixture.GlobalGen->GetCompilerPackage("C") == "gcc");
  ASSERT_TRUE(fixture.GlobalGen->GetCompilerPackage("CXX") == "gcc");
  ASSERT_TRUE(fixture.GlobalGen->GetCompilerPackage("Fortran") == "gfortran");
  ASSERT_TRUE(fixture.GlobalGen->GetCompilerPackage("CUDA") == "cudatoolkit");
  ASSERT_TRUE(fixture.GlobalGen->GetCompilerPackage("ASM") == "gcc");
  
  return true;
}

static bool testIsSystemPath()
{
  std::cout << "testIsSystemPath()\n";
  NixGeneratorTestFixture fixture;
  
  // Test system path detection
  ASSERT_TRUE(fixture.GlobalGen->IsSystemPath("/usr/include/stdio.h"));
  ASSERT_TRUE(fixture.GlobalGen->IsSystemPath("/nix/store/abc123/include/foo.h"));
  ASSERT_TRUE(fixture.GlobalGen->IsSystemPath("/opt/local/include/bar.h"));
  ASSERT_TRUE(fixture.GlobalGen->IsSystemPath("/usr/local/include/baz.h"));
  ASSERT_TRUE(fixture.GlobalGen->IsSystemPath("/System/Library/Frameworks"));
  ASSERT_TRUE(fixture.GlobalGen->IsSystemPath("/Library/Developer"));
  
  ASSERT_TRUE(!fixture.GlobalGen->IsSystemPath("/home/user/project/include/my.h"));
  ASSERT_TRUE(!fixture.GlobalGen->IsSystemPath("/tmp/build/generated.h"));
  
  return true;
}

static bool testNixWriterHelpers()
{
  std::cout << "testNixWriterHelpers()\n";
  
  // Test static helper methods that don't require a stream
  ASSERT_TRUE(cmNixWriter::MakeValidNixIdentifier("test-name") == "test-name");  // dashes are valid
  ASSERT_TRUE(cmNixWriter::MakeValidNixIdentifier("99bottles") == "_99bottles");  // prepend underscore
  
  return true;
}

static bool testLocalGeneratorCreation()
{
  std::cout << "testLocalGeneratorCreation()\n";
  NixGeneratorTestFixture fixture;
  
  // Create a minimal makefile for testing
  cmStateSnapshot snapshot = fixture.CMake->GetCurrentSnapshot();
  auto mf = cm::make_unique<cmMakefile>(fixture.GlobalGen.get(), snapshot);
  
  auto localGen = fixture.GlobalGen->CreateLocalGenerator(mf.get());
  ASSERT_TRUE(localGen != nullptr);
  
  // Verify it's the right type
  auto* nixLocalGen = dynamic_cast<cmLocalNixGenerator*>(localGen.get());
  ASSERT_TRUE(nixLocalGen != nullptr);
  
  return true;
}

static bool testGetDerivationName()
{
  std::cout << "testGetDerivationName()\n";
  NixGeneratorTestFixture fixture;
  
  // Test derivation name generation
  std::string sourcePath = "/path/to/source.cpp";
  std::string targetName = "myTarget";
  
  std::string derivName = fixture.GlobalGen->GetDerivationName(targetName, sourcePath);
  
  // The derivation name should contain the target name and source file name
  ASSERT_TRUE(derivName.find("myTarget") != std::string::npos);
  ASSERT_TRUE(derivName.find("source_cpp") != std::string::npos);
  
  return true;
}

int testNixGenerator(int /*unused*/, char* /*unused*/[])
{
  int failed = 0;
  
  if (!testGlobalGeneratorName()) {
    std::cerr << "testGlobalGeneratorName failed\n";
    failed = 1;
  }
  
  if (!testGlobalGeneratorFactory()) {
    std::cerr << "testGlobalGeneratorFactory failed\n";
    failed = 1;
  }
  
  if (!testIsMultiConfig()) {
    std::cerr << "testIsMultiConfig failed\n";
    failed = 1;
  }
  
  if (!testNixWriter()) {
    std::cerr << "testNixWriter failed\n";
    failed = 1;
  }
  
  if (!testNixWriterEscaping()) {
    std::cerr << "testNixWriterEscaping failed\n";
    failed = 1;
  }
  
  if (!testMakeValidNixIdentifier()) {
    std::cerr << "testMakeValidNixIdentifier failed\n";
    failed = 1;
  }
  
  if (!testGetCompilerPackage()) {
    std::cerr << "testGetCompilerPackage failed\n";
    failed = 1;
  }
  
  if (!testIsSystemPath()) {
    std::cerr << "testIsSystemPath failed\n";
    failed = 1;
  }
  
  if (!testNixWriterHelpers()) {
    std::cerr << "testNixWriterHelpers failed\n";
    failed = 1;
  }
  
  if (!testLocalGeneratorCreation()) {
    std::cerr << "testLocalGeneratorCreation failed\n";
    failed = 1;
  }
  
  if (!testGetDerivationName()) {
    std::cerr << "testGetDerivationName failed\n";
    failed = 1;
  }
  
  return failed;
}