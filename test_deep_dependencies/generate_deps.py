#!/usr/bin/env python3

# Generate 10 levels of dependencies with 5 libraries per level
levels = 10
libs_per_level = 5

cmake_content = """cmake_minimum_required(VERSION 3.20)
project(deep_dependencies_test CXX)

set(CMAKE_CXX_STANDARD 11)

"""

# Generate libraries for each level
for level in range(levels):
    for lib in range(libs_per_level):
        lib_name = f"lib_L{level}_N{lib}"
        cmake_content += f"# Level {level}, Library {lib}\n"
        cmake_content += f"add_library({lib_name} STATIC {lib_name}.cpp)\n"
        
        # Add dependencies to all libraries in the previous level
        if level > 0:
            cmake_content += f"target_link_libraries({lib_name} PRIVATE"
            for prev_lib in range(libs_per_level):
                cmake_content += f" lib_L{level-1}_N{prev_lib}"
            cmake_content += ")\n"
        
        cmake_content += "\n"
        
        # Generate source file
        with open(f"{lib_name}.cpp", "w") as f:
            f.write(f'#include <iostream>\n')
            f.write(f'int {lib_name}_func() {{\n')
            if level > 0:
                f.write(f'  int sum = {level * 100 + lib};\n')
                for prev_lib in range(libs_per_level):
                    f.write(f'  extern int lib_L{level-1}_N{prev_lib}_func();\n')
                    f.write(f'  sum += lib_L{level-1}_N{prev_lib}_func();\n')
                f.write(f'  return sum;\n')
            else:
                f.write(f'  return {lib + 1};\n')
            f.write(f'}}\n')

# Generate main executable that depends on top level
cmake_content += "# Main executable\n"
cmake_content += "add_executable(deep_test main.cpp)\n"
cmake_content += "target_link_libraries(deep_test PRIVATE"
for lib in range(libs_per_level):
    cmake_content += f" lib_L{levels-1}_N{lib}"
cmake_content += ")\n"

# Write CMakeLists.txt
with open("CMakeLists.txt", "w") as f:
    f.write(cmake_content)

# Generate main.cpp
with open("main.cpp", "w") as f:
    f.write('#include <iostream>\n')
    f.write('#include <chrono>\n\n')
    for lib in range(libs_per_level):
        f.write(f'extern int lib_L{levels-1}_N{lib}_func();\n')
    f.write('\nint main() {\n')
    f.write('  auto start = std::chrono::high_resolution_clock::now();\n')
    f.write('  int total = 0;\n')
    for lib in range(libs_per_level):
        f.write(f'  total += lib_L{levels-1}_N{lib}_func();\n')
    f.write('  auto end = std::chrono::high_resolution_clock::now();\n')
    f.write('  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);\n')
    f.write('  std::cout << "Deep dependency test completed." << std::endl;\n')
    f.write('  std::cout << "Total sum: " << total << std::endl;\n')
    f.write('  std::cout << "Execution time: " << duration.count() << " microseconds" << std::endl;\n')
    f.write('  return 0;\n')
    f.write('}\n')

print(f"Generated {levels} levels with {libs_per_level} libraries per level")
print(f"Total libraries: {levels * libs_per_level}")
print(f"Total dependencies: {sum((level * libs_per_level * libs_per_level) for level in range(1, levels))}")