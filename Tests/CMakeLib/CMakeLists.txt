# Render the header file with source path macro
configure_file(testConfig.h.in testConfig.h @ONLY)

include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMake_BINARY_DIR}/Source
  ${CMake_SOURCE_DIR}/Source
  ${CMake_SOURCE_DIR}/Source/CTest
  )

set(CMakeLib_TESTS
  testAssert.cxx
  testArgumentParser.cxx
  testCTestBinPacker.cxx
  testCTestResourceAllocator.cxx
  testCTestResourceSpec.cxx
  testCTestResourceGroups.cxx
  testDebug.cxx
  testDocumentationFormatter.cxx
  testGccDepfileReader.cxx
  testGeneratedFileStream.cxx
  testJSONHelpers.cxx
  testRST.cxx
  testRange.cxx
  testOptional.cxx
  testPathResolver.cxx
  testSPDXSerializer.cxx
  testStdIo.cxx
  testString.cxx
  testStringAlgorithms.cxx
  testSystemTools.cxx
  testUTF8.cxx
  testXMLParser.cxx
  testXMLSafe.cxx
  testFindPackageCommand.cxx
  testUVHandlePtr.cxx
  testUVJobServerClient.cxx
  testUVPatches.cxx
  testUVProcessChain.cxx
  testUVRAII.cxx
  testUVStreambuf.cxx
  testCMExtMemory.cxx
  testCMExtAlgorithm.cxx
  testCMExtEnumSet.cxx
  testList.cxx
  testCMakePath.cxx
  testNixGenerator.cxx
  testNixComponentRefactoring.cxx
  testNixSecurityPaths.cxx
  testNixThreadSafety.cxx
  testNixErrorRecovery.cxx
  testNixEdgeCases.cxx
  )
if(CMake_ENABLE_DEBUGGER)
  list(APPEND CMakeLib_TESTS
    testDebuggerAdapter.cxx
    testDebuggerAdapterPipe.cxx
    testDebuggerBreakpointManager.cxx
    testDebuggerVariables.cxx
    testDebuggerVariablesHelper.cxx
    testDebuggerVariablesManager.cxx
    testDebuggerThread.cxx
    )
endif()
if (CMake_TEST_FILESYSTEM_PATH OR NOT CMake_HAVE_CXX_FILESYSTEM)
  list(APPEND CMakeLib_TESTS testCMFilesystemPath.cxx)
endif()

add_executable(testUVProcessChainHelper testUVProcessChainHelper.cxx)
target_link_libraries(testUVProcessChainHelper CMakeLib)
target_compile_definitions(testUVProcessChainHelper PRIVATE WIN32_LEAN_AND_MEAN)

set(testRST_ARGS ${CMAKE_CURRENT_SOURCE_DIR})
set(testUVProcessChain_ARGS $<TARGET_FILE:testUVProcessChainHelper>)
set(testUVStreambuf_ARGS $<TARGET_FILE:cmake>)
set(testCTestResourceSpec_ARGS ${CMAKE_CURRENT_SOURCE_DIR})
set(testGccDepfileReader_ARGS ${CMAKE_CURRENT_SOURCE_DIR})

if(WIN32)
  list(APPEND CMakeLib_TESTS
    testVisualStudioSlnParser.cxx
    )
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/testUVProcessChainInput.txt" "HELLO WORLD!")

create_test_sourcelist(CMakeLib_TEST_SRCS CMakeLibTests.cxx ${CMakeLib_TESTS})
add_executable(CMakeLibTests ${CMakeLib_TEST_SRCS})
target_link_libraries(CMakeLibTests PRIVATE CTestLib CMakeLib)
if(CMake_BUILD_PCH)
  target_precompile_headers(CMakeLibTests PRIVATE "<iostream>" "<cm3p/uv.h>")
  target_compile_definitions(CMakeLibTests PRIVATE "NOMINMAX")
endif()
if(WIN32)
  target_compile_definitions(CMakeLibTests PRIVATE WIN32_LEAN_AND_MEAN)
endif()

set_property(TARGET CMakeLibTests PROPERTY C_CLANG_TIDY "")
set_property(TARGET CMakeLibTests PROPERTY CXX_CLANG_TIDY "")

add_executable(testEncoding testEncoding.cxx)
target_link_libraries(testEncoding cmsys)

foreach(testfile ${CMakeLib_TESTS})
  get_filename_component(test "${testfile}" NAME_WE)
  add_test(NAME CMakeLib.${test} COMMAND CMakeLibTests ${test} ${${test}_ARGS})
endforeach()

if(TEST_CompileCommandOutput)
  add_executable(runcompilecommands run_compile_commands.cxx)
  target_link_libraries(runcompilecommands CMakeLib)
endif()

add_subdirectory(PseudoMemcheck)

add_executable(testAffinity testAffinity.cxx)
target_link_libraries(testAffinity CMakeLib)

if(CMake_ENABLE_DEBUGGER)
  add_executable(testDebuggerNamedPipe testDebuggerNamedPipe.cxx)
  target_link_libraries(testDebuggerNamedPipe PRIVATE CMakeLib)
  set(testDebuggerNamedPipe_Project_ARGS
    "$<TARGET_FILE:cmake>" -S ${CMAKE_CURRENT_SOURCE_DIR}/DebuggerSample -B ${CMAKE_CURRENT_BINARY_DIR}/DebuggerSample
    --fresh -G ${CMAKE_GENERATOR}
    )
  if(CMAKE_GENERATOR_PLATFORM)
    list(APPEND testDebuggerNamedPipe_Project_ARGS -A "${CMAKE_GENERATOR_PLATFORM}")
  endif()
  if(CMAKE_GENERATOR_TOOLSET)
    list(APPEND testDebuggerNamedPipe_Project_ARGS -T "${CMAKE_GENERATOR_TOOLSET}")
  endif()
  if(CMAKE_GENERATOR_INSTANCE)
    list(APPEND testDebuggerNamedPipe_Project_ARGS "-DCMAKE_GENERATOR_INSTANCE=${CMAKE_GENERATOR_INSTANCE}")
  endif()
  set(testDebuggerNamedPipe_Script_ARGS
    "$<TARGET_FILE:cmake>" -P ${CMAKE_CURRENT_SOURCE_DIR}/DebuggerSample/script.cmake
    )
  foreach(case Project Script)
    add_test(NAME CMakeLib.testDebuggerNamedPipe-${case} COMMAND testDebuggerNamedPipe ${testDebuggerNamedPipe_${case}_ARGS})
    set_property(TEST CMakeLib.testDebuggerNamedPipe-${case} PROPERTY TIMEOUT 300)
  endforeach()
  if(WIN32)
    target_compile_definitions(testDebuggerNamedPipe PRIVATE WIN32_LEAN_AND_MEAN)
  endif()
endif()
