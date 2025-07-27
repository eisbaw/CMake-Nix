cmake_minimum_required(VERSION 3.20)
project(ExportImportTest CXX)

# Enable exports
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Create a library that will be exported
add_library(mylib STATIC src/mylib.cpp)
target_include_directories(mylib PUBLIC 
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

# Create an executable that uses the library
add_executable(myapp src/main.cpp)
target_link_libraries(myapp PRIVATE mylib)

# Export targets for use by other projects
export(TARGETS mylib myapp
  FILE "${CMAKE_CURRENT_BINARY_DIR}/ExportImportTestTargets.cmake"
  NAMESPACE ExportImportTest::
)

# Also test export with EXPORT command
install(TARGETS mylib myapp
  EXPORT ExportImportTestTargets
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

install(EXPORT ExportImportTestTargets
  FILE ExportImportTestTargets.cmake
  NAMESPACE ExportImportTest::
  DESTINATION lib/cmake/ExportImportTest
)

# Create package config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/ExportImportTestConfigVersion.cmake"
  VERSION 1.0.0
  COMPATIBILITY AnyNewerVersion
)

configure_file(ExportImportTestConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/ExportImportTestConfig.cmake"
  @ONLY
)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/ExportImportTestConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/ExportImportTestConfigVersion.cmake"
  DESTINATION lib/cmake/ExportImportTest
)