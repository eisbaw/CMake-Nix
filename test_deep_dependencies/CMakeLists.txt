cmake_minimum_required(VERSION 3.20)
project(deep_dependencies_test CXX)

set(CMAKE_CXX_STANDARD 11)

# Level 0, Library 0
add_library(lib_L0_N0 STATIC lib_L0_N0.cpp)

# Level 0, Library 1
add_library(lib_L0_N1 STATIC lib_L0_N1.cpp)

# Level 0, Library 2
add_library(lib_L0_N2 STATIC lib_L0_N2.cpp)

# Level 0, Library 3
add_library(lib_L0_N3 STATIC lib_L0_N3.cpp)

# Level 0, Library 4
add_library(lib_L0_N4 STATIC lib_L0_N4.cpp)

# Level 1, Library 0
add_library(lib_L1_N0 STATIC lib_L1_N0.cpp)
target_link_libraries(lib_L1_N0 PRIVATE lib_L0_N0 lib_L0_N1 lib_L0_N2 lib_L0_N3 lib_L0_N4)

# Level 1, Library 1
add_library(lib_L1_N1 STATIC lib_L1_N1.cpp)
target_link_libraries(lib_L1_N1 PRIVATE lib_L0_N0 lib_L0_N1 lib_L0_N2 lib_L0_N3 lib_L0_N4)

# Level 1, Library 2
add_library(lib_L1_N2 STATIC lib_L1_N2.cpp)
target_link_libraries(lib_L1_N2 PRIVATE lib_L0_N0 lib_L0_N1 lib_L0_N2 lib_L0_N3 lib_L0_N4)

# Level 1, Library 3
add_library(lib_L1_N3 STATIC lib_L1_N3.cpp)
target_link_libraries(lib_L1_N3 PRIVATE lib_L0_N0 lib_L0_N1 lib_L0_N2 lib_L0_N3 lib_L0_N4)

# Level 1, Library 4
add_library(lib_L1_N4 STATIC lib_L1_N4.cpp)
target_link_libraries(lib_L1_N4 PRIVATE lib_L0_N0 lib_L0_N1 lib_L0_N2 lib_L0_N3 lib_L0_N4)

# Level 2, Library 0
add_library(lib_L2_N0 STATIC lib_L2_N0.cpp)
target_link_libraries(lib_L2_N0 PRIVATE lib_L1_N0 lib_L1_N1 lib_L1_N2 lib_L1_N3 lib_L1_N4)

# Level 2, Library 1
add_library(lib_L2_N1 STATIC lib_L2_N1.cpp)
target_link_libraries(lib_L2_N1 PRIVATE lib_L1_N0 lib_L1_N1 lib_L1_N2 lib_L1_N3 lib_L1_N4)

# Level 2, Library 2
add_library(lib_L2_N2 STATIC lib_L2_N2.cpp)
target_link_libraries(lib_L2_N2 PRIVATE lib_L1_N0 lib_L1_N1 lib_L1_N2 lib_L1_N3 lib_L1_N4)

# Level 2, Library 3
add_library(lib_L2_N3 STATIC lib_L2_N3.cpp)
target_link_libraries(lib_L2_N3 PRIVATE lib_L1_N0 lib_L1_N1 lib_L1_N2 lib_L1_N3 lib_L1_N4)

# Level 2, Library 4
add_library(lib_L2_N4 STATIC lib_L2_N4.cpp)
target_link_libraries(lib_L2_N4 PRIVATE lib_L1_N0 lib_L1_N1 lib_L1_N2 lib_L1_N3 lib_L1_N4)

# Level 3, Library 0
add_library(lib_L3_N0 STATIC lib_L3_N0.cpp)
target_link_libraries(lib_L3_N0 PRIVATE lib_L2_N0 lib_L2_N1 lib_L2_N2 lib_L2_N3 lib_L2_N4)

# Level 3, Library 1
add_library(lib_L3_N1 STATIC lib_L3_N1.cpp)
target_link_libraries(lib_L3_N1 PRIVATE lib_L2_N0 lib_L2_N1 lib_L2_N2 lib_L2_N3 lib_L2_N4)

# Level 3, Library 2
add_library(lib_L3_N2 STATIC lib_L3_N2.cpp)
target_link_libraries(lib_L3_N2 PRIVATE lib_L2_N0 lib_L2_N1 lib_L2_N2 lib_L2_N3 lib_L2_N4)

# Level 3, Library 3
add_library(lib_L3_N3 STATIC lib_L3_N3.cpp)
target_link_libraries(lib_L3_N3 PRIVATE lib_L2_N0 lib_L2_N1 lib_L2_N2 lib_L2_N3 lib_L2_N4)

# Level 3, Library 4
add_library(lib_L3_N4 STATIC lib_L3_N4.cpp)
target_link_libraries(lib_L3_N4 PRIVATE lib_L2_N0 lib_L2_N1 lib_L2_N2 lib_L2_N3 lib_L2_N4)

# Level 4, Library 0
add_library(lib_L4_N0 STATIC lib_L4_N0.cpp)
target_link_libraries(lib_L4_N0 PRIVATE lib_L3_N0 lib_L3_N1 lib_L3_N2 lib_L3_N3 lib_L3_N4)

# Level 4, Library 1
add_library(lib_L4_N1 STATIC lib_L4_N1.cpp)
target_link_libraries(lib_L4_N1 PRIVATE lib_L3_N0 lib_L3_N1 lib_L3_N2 lib_L3_N3 lib_L3_N4)

# Level 4, Library 2
add_library(lib_L4_N2 STATIC lib_L4_N2.cpp)
target_link_libraries(lib_L4_N2 PRIVATE lib_L3_N0 lib_L3_N1 lib_L3_N2 lib_L3_N3 lib_L3_N4)

# Level 4, Library 3
add_library(lib_L4_N3 STATIC lib_L4_N3.cpp)
target_link_libraries(lib_L4_N3 PRIVATE lib_L3_N0 lib_L3_N1 lib_L3_N2 lib_L3_N3 lib_L3_N4)

# Level 4, Library 4
add_library(lib_L4_N4 STATIC lib_L4_N4.cpp)
target_link_libraries(lib_L4_N4 PRIVATE lib_L3_N0 lib_L3_N1 lib_L3_N2 lib_L3_N3 lib_L3_N4)

# Level 5, Library 0
add_library(lib_L5_N0 STATIC lib_L5_N0.cpp)
target_link_libraries(lib_L5_N0 PRIVATE lib_L4_N0 lib_L4_N1 lib_L4_N2 lib_L4_N3 lib_L4_N4)

# Level 5, Library 1
add_library(lib_L5_N1 STATIC lib_L5_N1.cpp)
target_link_libraries(lib_L5_N1 PRIVATE lib_L4_N0 lib_L4_N1 lib_L4_N2 lib_L4_N3 lib_L4_N4)

# Level 5, Library 2
add_library(lib_L5_N2 STATIC lib_L5_N2.cpp)
target_link_libraries(lib_L5_N2 PRIVATE lib_L4_N0 lib_L4_N1 lib_L4_N2 lib_L4_N3 lib_L4_N4)

# Level 5, Library 3
add_library(lib_L5_N3 STATIC lib_L5_N3.cpp)
target_link_libraries(lib_L5_N3 PRIVATE lib_L4_N0 lib_L4_N1 lib_L4_N2 lib_L4_N3 lib_L4_N4)

# Level 5, Library 4
add_library(lib_L5_N4 STATIC lib_L5_N4.cpp)
target_link_libraries(lib_L5_N4 PRIVATE lib_L4_N0 lib_L4_N1 lib_L4_N2 lib_L4_N3 lib_L4_N4)

# Level 6, Library 0
add_library(lib_L6_N0 STATIC lib_L6_N0.cpp)
target_link_libraries(lib_L6_N0 PRIVATE lib_L5_N0 lib_L5_N1 lib_L5_N2 lib_L5_N3 lib_L5_N4)

# Level 6, Library 1
add_library(lib_L6_N1 STATIC lib_L6_N1.cpp)
target_link_libraries(lib_L6_N1 PRIVATE lib_L5_N0 lib_L5_N1 lib_L5_N2 lib_L5_N3 lib_L5_N4)

# Level 6, Library 2
add_library(lib_L6_N2 STATIC lib_L6_N2.cpp)
target_link_libraries(lib_L6_N2 PRIVATE lib_L5_N0 lib_L5_N1 lib_L5_N2 lib_L5_N3 lib_L5_N4)

# Level 6, Library 3
add_library(lib_L6_N3 STATIC lib_L6_N3.cpp)
target_link_libraries(lib_L6_N3 PRIVATE lib_L5_N0 lib_L5_N1 lib_L5_N2 lib_L5_N3 lib_L5_N4)

# Level 6, Library 4
add_library(lib_L6_N4 STATIC lib_L6_N4.cpp)
target_link_libraries(lib_L6_N4 PRIVATE lib_L5_N0 lib_L5_N1 lib_L5_N2 lib_L5_N3 lib_L5_N4)

# Level 7, Library 0
add_library(lib_L7_N0 STATIC lib_L7_N0.cpp)
target_link_libraries(lib_L7_N0 PRIVATE lib_L6_N0 lib_L6_N1 lib_L6_N2 lib_L6_N3 lib_L6_N4)

# Level 7, Library 1
add_library(lib_L7_N1 STATIC lib_L7_N1.cpp)
target_link_libraries(lib_L7_N1 PRIVATE lib_L6_N0 lib_L6_N1 lib_L6_N2 lib_L6_N3 lib_L6_N4)

# Level 7, Library 2
add_library(lib_L7_N2 STATIC lib_L7_N2.cpp)
target_link_libraries(lib_L7_N2 PRIVATE lib_L6_N0 lib_L6_N1 lib_L6_N2 lib_L6_N3 lib_L6_N4)

# Level 7, Library 3
add_library(lib_L7_N3 STATIC lib_L7_N3.cpp)
target_link_libraries(lib_L7_N3 PRIVATE lib_L6_N0 lib_L6_N1 lib_L6_N2 lib_L6_N3 lib_L6_N4)

# Level 7, Library 4
add_library(lib_L7_N4 STATIC lib_L7_N4.cpp)
target_link_libraries(lib_L7_N4 PRIVATE lib_L6_N0 lib_L6_N1 lib_L6_N2 lib_L6_N3 lib_L6_N4)

# Level 8, Library 0
add_library(lib_L8_N0 STATIC lib_L8_N0.cpp)
target_link_libraries(lib_L8_N0 PRIVATE lib_L7_N0 lib_L7_N1 lib_L7_N2 lib_L7_N3 lib_L7_N4)

# Level 8, Library 1
add_library(lib_L8_N1 STATIC lib_L8_N1.cpp)
target_link_libraries(lib_L8_N1 PRIVATE lib_L7_N0 lib_L7_N1 lib_L7_N2 lib_L7_N3 lib_L7_N4)

# Level 8, Library 2
add_library(lib_L8_N2 STATIC lib_L8_N2.cpp)
target_link_libraries(lib_L8_N2 PRIVATE lib_L7_N0 lib_L7_N1 lib_L7_N2 lib_L7_N3 lib_L7_N4)

# Level 8, Library 3
add_library(lib_L8_N3 STATIC lib_L8_N3.cpp)
target_link_libraries(lib_L8_N3 PRIVATE lib_L7_N0 lib_L7_N1 lib_L7_N2 lib_L7_N3 lib_L7_N4)

# Level 8, Library 4
add_library(lib_L8_N4 STATIC lib_L8_N4.cpp)
target_link_libraries(lib_L8_N4 PRIVATE lib_L7_N0 lib_L7_N1 lib_L7_N2 lib_L7_N3 lib_L7_N4)

# Level 9, Library 0
add_library(lib_L9_N0 STATIC lib_L9_N0.cpp)
target_link_libraries(lib_L9_N0 PRIVATE lib_L8_N0 lib_L8_N1 lib_L8_N2 lib_L8_N3 lib_L8_N4)

# Level 9, Library 1
add_library(lib_L9_N1 STATIC lib_L9_N1.cpp)
target_link_libraries(lib_L9_N1 PRIVATE lib_L8_N0 lib_L8_N1 lib_L8_N2 lib_L8_N3 lib_L8_N4)

# Level 9, Library 2
add_library(lib_L9_N2 STATIC lib_L9_N2.cpp)
target_link_libraries(lib_L9_N2 PRIVATE lib_L8_N0 lib_L8_N1 lib_L8_N2 lib_L8_N3 lib_L8_N4)

# Level 9, Library 3
add_library(lib_L9_N3 STATIC lib_L9_N3.cpp)
target_link_libraries(lib_L9_N3 PRIVATE lib_L8_N0 lib_L8_N1 lib_L8_N2 lib_L8_N3 lib_L8_N4)

# Level 9, Library 4
add_library(lib_L9_N4 STATIC lib_L9_N4.cpp)
target_link_libraries(lib_L9_N4 PRIVATE lib_L8_N0 lib_L8_N1 lib_L8_N2 lib_L8_N3 lib_L8_N4)

# Main executable
add_executable(deep_test main.cpp)
target_link_libraries(deep_test PRIVATE lib_L9_N0 lib_L9_N1 lib_L9_N2 lib_L9_N3 lib_L9_N4)
