#!/usr/bin/env python3
"""Generate source files for thread safety test."""

import os

# Create src directory if it doesn't exist
os.makedirs("src", exist_ok=True)

# Generate library sources
for i in range(1, 11):
    content = f"""#include <stdio.h>

void lib{i}_function() {{
    printf("Library {i} function called\\n");
}}

int lib{i}_compute(int x) {{
    // Simulate some computation
    int result = x;
    for (int j = 0; j < 1000; j++) {{
        result = (result * {i} + j) % 1000000;
    }}
    return result;
}}
"""
    with open(f"src/lib{i}.c", "w") as f:
        f.write(content)

# Generate app1.c
with open("src/app1.c", "w") as f:
    f.write("""#include <stdio.h>

void lib1_function();
void lib2_function();
void lib3_function();

int main() {
    printf("App1 starting...\\n");
    lib1_function();
    lib2_function();
    lib3_function();
    printf("App1 completed!\\n");
    return 0;
}
""")

# Generate app2.c
with open("src/app2.c", "w") as f:
    f.write("""#include <stdio.h>

void lib4_function();
void lib5_function();
void lib6_function();

int main() {
    printf("App2 starting...\\n");
    lib4_function();
    lib5_function();
    lib6_function();
    printf("App2 completed!\\n");
    return 0;
}
""")

# Generate app3.c
with open("src/app3.c", "w") as f:
    f.write("""#include <stdio.h>

void lib7_function();
void lib8_function();
void lib9_function();
void lib10_function();

int main() {
    printf("App3 starting...\\n");
    lib7_function();
    lib8_function();
    lib9_function();
    lib10_function();
    printf("App3 completed!\\n");
    return 0;
}
""")

# Generate final.c
with open("src/final.c", "w") as f:
    f.write("""#include <stdio.h>

int main() {
    printf("Final app - all dependencies built successfully!\\n");
    printf("Thread safety test passed if this built without errors.\\n");
    return 0;
}
""")

print("Generated all source files for thread safety test")