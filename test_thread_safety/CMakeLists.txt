cmake_minimum_required(VERSION 3.10)
project(test_thread_safety C)

# Enable parallel builds
set(CMAKE_JOB_POOL_COMPILE compile_pool)
set_property(GLOBAL PROPERTY JOB_POOLS compile_pool=4)

# Create multiple independent libraries that can be built in parallel
foreach(i RANGE 1 10)
    add_library(lib${i} STATIC src/lib${i}.c)
endforeach()

# Create executables that depend on different combinations of libraries
add_executable(app1 src/app1.c)
target_link_libraries(app1 lib1 lib2 lib3)

add_executable(app2 src/app2.c)  
target_link_libraries(app2 lib4 lib5 lib6)

add_executable(app3 src/app3.c)
target_link_libraries(app3 lib7 lib8 lib9 lib10)

# Create a final app that depends on all others (stress test dependencies)
add_executable(final_app src/final.c)
add_dependencies(final_app app1 app2 app3)