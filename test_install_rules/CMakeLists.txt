cmake_minimum_required(VERSION 3.20)
project(InstallRulesTest C)

# Create a shared library
add_library(install_lib SHARED lib.c)

# Create a static library
add_library(install_static STATIC static.c)

# Create a simple executable that uses the libraries
add_executable(install_app main.c)
target_link_libraries(install_app PRIVATE install_lib install_static)

# Add install rules for testing
install(TARGETS install_app DESTINATION bin)
install(TARGETS install_lib DESTINATION lib)
install(TARGETS install_static DESTINATION lib)

# Set some install-related properties to test detection
set_target_properties(install_app PROPERTIES
    INSTALL_RPATH "\$ORIGIN/../lib"
)

set_target_properties(install_lib PROPERTIES
    EXPORT_NAME "InstallLib"
    VERSION 1.0.0
    SOVERSION 1
)