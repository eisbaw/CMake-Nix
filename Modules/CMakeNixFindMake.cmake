# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

find_program(CMAKE_MAKE_PROGRAM
  NAMES nix-build
  NAMES_PER_DIR
  DOC "Program used to build from default.nix files.")

# If nix-build is not found in PATH, default to "nix-build" as the program name
# This allows the Nix generator to work without requiring -DCMAKE_MAKE_PROGRAM=nix-build
if(NOT CMAKE_MAKE_PROGRAM)
  set(CMAKE_MAKE_PROGRAM "nix-build" CACHE FILEPATH "Program used to build from default.nix files." FORCE)
  message(STATUS "CMAKE_MAKE_PROGRAM not found in PATH, defaulting to: ${CMAKE_MAKE_PROGRAM}")
endif()

mark_as_advanced(CMAKE_MAKE_PROGRAM)