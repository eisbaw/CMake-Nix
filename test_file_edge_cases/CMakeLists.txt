cmake_minimum_required(VERSION 3.20)
project(TestFileEdgeCases C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Test 1: Files with spaces in names
add_executable(test_spaces "source with spaces.cpp")

# Test 2: Target with special characters
add_executable("test-with-dashes" special_chars.cpp)
add_executable(test_with_underscores special_chars.cpp)
add_executable(test.with.dots special_chars.cpp)

# Test 3: Files with unicode in paths (if filesystem supports it)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unicode_文件.cpp")
  add_executable(test_unicode "unicode_文件.cpp")
else()
  message(STATUS "Unicode filename test skipped - filesystem may not support unicode")
endif()

# Test 4: Very long filenames
add_executable(test_long_names "this_is_a_very_long_filename_that_might_cause_issues_with_some_build_systems_but_should_work_fine.cpp")

# Test 5: Symlinks (create at configure time)
execute_process(
  COMMAND ${CMAKE_COMMAND} -E create_symlink 
    "${CMAKE_CURRENT_SOURCE_DIR}/special_chars.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/symlinked_source.cpp"
  RESULT_VARIABLE symlink_result
)

if(symlink_result EQUAL 0)
  add_executable(test_symlinks symlinked_source.cpp)
else()
  message(STATUS "Symlink test skipped - unable to create symlinks")
endif()

# Test 6: Mixed case sensitivity
add_executable(TestMixedCase MixedCase.cpp)

# Test 7: Files in deeply nested directories
file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/very/deeply/nested/directory/structure")
configure_file(special_chars.cpp "very/deeply/nested/directory/structure/nested.cpp" COPYONLY)
add_executable(test_nested "very/deeply/nested/directory/structure/nested.cpp")

# Test 8: Special characters in directory names
file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/dir with spaces")
configure_file(special_chars.cpp "dir with spaces/file.cpp" COPYONLY)
add_executable(test_dir_spaces "dir with spaces/file.cpp")

# Add include directories with special characters
target_include_directories(test_spaces PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include with spaces")
target_include_directories(test_dir_spaces PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include with spaces")

# Custom command with special characters
add_custom_command(
  TARGET test_spaces POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E echo "Built target with spaces in filename"
)