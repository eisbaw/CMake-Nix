cmake_minimum_required(VERSION 3.20)
project(AsmLanguageTest C ASM)

# Enable ASM language
enable_language(ASM)

# Test basic ASM compilation
add_executable(asm_test main.c hello.s)