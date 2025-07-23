cmake_minimum_required(VERSION 3.20)

# Allow the project to configure even without Fortran compiler
project(FortranTest LANGUAGES NONE)

# Enable Fortran if available
include(CheckLanguage)
check_language(Fortran)

if(CMAKE_Fortran_COMPILER)
    enable_language(Fortran)
    enable_language(C)
    
    # Simple Fortran executable
    add_executable(fortran_hello hello.f90)
    
    # Mixed Fortran and C
    add_executable(mixed_fortran mixed_main.c fortran_calc.f90)
else()
    message(STATUS "Fortran compiler not found - creating placeholder targets")
    
    # Create dummy custom targets that generate Nix expressions
    add_custom_target(fortran_hello ALL
        COMMAND ${CMAKE_COMMAND} -E echo "Fortran compiler would be needed for this target"
        SOURCES hello.f90
    )
    
    add_custom_target(mixed_fortran ALL  
        COMMAND ${CMAKE_COMMAND} -E echo "Fortran compiler would be needed for this target"
        SOURCES mixed_main.c fortran_calc.f90
    )
endif()