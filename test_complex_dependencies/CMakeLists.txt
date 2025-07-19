cmake_minimum_required(VERSION 3.20)
project(ComplexDependenciesTest CXX)

# Base utility library (no dependencies)
add_library(base_utils STATIC base_utils.cpp)
target_include_directories(base_utils PUBLIC include)

# Math library (depends on base_utils)
add_library(math_lib SHARED math_lib.cpp)
target_link_libraries(math_lib PUBLIC base_utils)
target_include_directories(math_lib PUBLIC include)

# Graphics library (depends on math_lib, which transitively depends on base_utils)
add_library(graphics_lib STATIC graphics_lib.cpp)
target_link_libraries(graphics_lib PUBLIC math_lib)
target_include_directories(graphics_lib PUBLIC include)

# Engine library (depends on both graphics_lib and math_lib directly)
add_library(engine_lib SHARED engine_lib.cpp)
target_link_libraries(engine_lib PUBLIC graphics_lib math_lib)
target_include_directories(engine_lib PUBLIC include)

# Application that uses engine (tests transitive dependencies)
add_executable(complex_app main.cpp)
target_link_libraries(complex_app PRIVATE engine_lib)

# Another executable that uses libraries directly (tests multiple direct dependencies)
add_executable(direct_app direct_main.cpp)
target_link_libraries(direct_app PRIVATE graphics_lib math_lib base_utils)

# Library versioning
set_target_properties(math_lib PROPERTIES
    VERSION 2.1.0
    SOVERSION 2
    EXPORT_NAME "MathLib"
)

set_target_properties(engine_lib PROPERTIES
    VERSION 1.3.5
    SOVERSION 1
    EXPORT_NAME "EngineLib"
)

# Install everything with dependencies
install(TARGETS complex_app direct_app engine_lib graphics_lib math_lib base_utils
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)