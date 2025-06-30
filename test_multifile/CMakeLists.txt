cmake_minimum_required(VERSION 3.13)
project(MultiFileTest LANGUAGES C)

add_executable(calculator main.c math.c)
add_library(mathlib STATIC math.c utils.c)