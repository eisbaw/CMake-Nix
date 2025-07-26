#!/usr/bin/env python3
"""Generate many source files to test CMake Nix backend at scale."""

import os
import sys

def generate_header(index, deps=None):
    """Generate a header file with optional dependencies."""
    deps = deps or []
    content = f"""#pragma once
#ifndef HEADER_{index}_H
#define HEADER_{index}_H

#include <stdio.h>

"""
    for dep in deps:
        content += f'#include "header_{dep}.h"\n'
    
    content += f"""
// Function declaration
int function_{index}(int x);

// Inline function  
static inline int inline_func_{index}(int x) {{
    return x * {index};
}}

#endif // HEADER_{index}_H
"""
    return content

def generate_source(index, deps=None):
    """Generate a source file with optional header dependencies."""
    deps = deps or []
    content = f"""#include "header_{index}.h"
"""
    for dep in deps:
        content += f'#include "header_{dep}.h"\n'
    
    content += f"""
int function_{index}(int x) {{
    int result = inline_func_{index}(x);
"""
    
    # Call dependent functions
    for i, dep in enumerate(deps):
        content += f"    result += function_{dep}(x + {i});\n"
    
    content += f"""    return result;
}}
"""
    return content

def generate_main(num_files):
    """Generate main.c that calls all functions."""
    content = """#include <stdio.h>
#include <time.h>

// Include all headers
"""
    for i in range(num_files):
        content += f'#include "header_{i}.h"\n'
    
    content += """
int main() {
    printf("Scale test with %d source files\\n", """ + str(num_files) + """);
    printf("=================================\\n\\n");
    
    clock_t start = clock();
    int result = 0;
    
    // Call all functions
"""
    for i in range(min(10, num_files)):  # Only call first 10 to avoid too long runtime
        content += f"    result += function_{i}(1);\n"
    
    content += """
    clock_t end = clock();
    double cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    
    printf("Result: %d\\n", result);
    printf("Execution time: %f seconds\\n", cpu_time_used);
    
    return 0;
}
"""
    return content

def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <number_of_files>")
        sys.exit(1)
    
    num_files = int(sys.argv[1])
    print(f"Generating {num_files} source files...")
    
    # Generate headers
    for i in range(num_files):
        # Create dependencies - each file depends on previous 2
        deps = []
        if i > 0:
            deps.append(i - 1)
        if i > 1 and i % 3 == 0:  # Some files depend on 2 headers
            deps.append(i - 2)
        
        with open(f"include/header_{i}.h", "w") as f:
            f.write(generate_header(i, deps))
    
    # Generate sources
    for i in range(num_files):
        # Same dependencies as headers
        deps = []
        if i > 0:
            deps.append(i - 1)
        if i > 1 and i % 3 == 0:
            deps.append(i - 2)
        
        with open(f"src/source_{i}.c", "w") as f:
            f.write(generate_source(i, deps))
    
    # Generate main
    with open("src/main.c", "w") as f:
        f.write(generate_main(num_files))
    
    # Generate CMakeLists.txt
    cmake_content = f"""cmake_minimum_required(VERSION 3.10)
project(scale_test C)

# Add include directory
include_directories(include)

# Add all source files
set(SOURCES
    src/main.c
"""
    for i in range(num_files):
        cmake_content += f"    src/source_{i}.c\n"
    
    cmake_content += """)

# Create executable
add_executable(scale_test ${SOURCES})

# Set C standard
set_property(TARGET scale_test PROPERTY C_STANDARD 99)
"""
    
    with open("CMakeLists.txt", "w") as f:
        f.write(cmake_content)
    
    print(f"Generated {num_files} headers in include/")
    print(f"Generated {num_files} sources in src/")
    print("Generated main.c and CMakeLists.txt")

if __name__ == "__main__":
    main()