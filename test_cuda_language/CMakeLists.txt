cmake_minimum_required(VERSION 3.20)
project(CudaTest LANGUAGES CUDA CXX)

# Set CUDA standard
set(CMAKE_CUDA_STANDARD 11)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

# Pure CUDA executable
add_executable(cuda_hello hello.cu)

# Mixed CUDA and C++
add_executable(mixed_cuda main.cpp cuda_kernels.cu)