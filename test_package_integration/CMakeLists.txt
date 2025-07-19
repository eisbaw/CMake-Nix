cmake_minimum_required(VERSION 3.20)
project(PackageIntegrationTest CXX)

# Try to find multiple common packages
find_package(Threads REQUIRED)

# Try optional packages (won't fail if not found)
find_package(ZLIB)
find_package(OpenSSL)
find_package(CURL)
find_package(PNG)

# Create library that uses threading
add_library(threaded_lib SHARED threaded_lib.cpp)
target_link_libraries(threaded_lib PUBLIC Threads::Threads)

# Create main application
add_executable(integration_app main.cpp)
target_link_libraries(integration_app PRIVATE threaded_lib)

# Conditionally link optional libraries
if(ZLIB_FOUND)
    target_link_libraries(integration_app PRIVATE ZLIB::ZLIB)
    target_compile_definitions(integration_app PRIVATE HAVE_ZLIB=1)
    message(STATUS "ZLIB found and will be linked")
endif()

if(OPENSSL_FOUND)
    target_link_libraries(integration_app PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    target_compile_definitions(integration_app PRIVATE HAVE_OPENSSL=1)
    message(STATUS "OpenSSL found and will be linked")
endif()

if(CURL_FOUND)
    target_link_libraries(integration_app PRIVATE CURL::libcurl)
    target_compile_definitions(integration_app PRIVATE HAVE_CURL=1)
    message(STATUS "CURL found and will be linked")
endif()

if(PNG_FOUND)
    target_link_libraries(integration_app PRIVATE PNG::PNG)
    target_compile_definitions(integration_app PRIVATE HAVE_PNG=1)
    message(STATUS "PNG found and will be linked")
endif()

# Install with dependencies
install(TARGETS integration_app threaded_lib
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib)