cmake_minimum_required(VERSION 3.20)
project(MixedLanguageTest C CXX)

# C library with external interface
add_library(c_math STATIC c_math.c)

# C++ library that uses the C library
add_library(cpp_utils SHARED cpp_utils.cpp)
target_link_libraries(cpp_utils PRIVATE c_math)

# Mixed application that uses both
add_executable(mixed_app main.cpp c_interface.c)
target_link_libraries(mixed_app PRIVATE cpp_utils c_math)

# Set different compile options for C and C++
set_property(TARGET c_math PROPERTY C_STANDARD 99)
set_property(TARGET cpp_utils PROPERTY CXX_STANDARD 17)
set_property(TARGET mixed_app PROPERTY C_STANDARD 99)
set_property(TARGET mixed_app PROPERTY CXX_STANDARD 17)

# Install everything
install(TARGETS mixed_app cpp_utils c_math
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)