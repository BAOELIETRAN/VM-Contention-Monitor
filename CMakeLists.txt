# A CMake file describes how a C++ project should be built.
# == It generates scripts for other build automation tools.
# CMake generates build files (e.g., Makefiles, Ninja files, ...) - Makefiles build project.

# cmake version >= 3.14 (must)
# this project needs CMAKE version >= 3.14
cmake_minimum_required(VERSION 3.14)

# project declaration
# name: VM_MONITOR
# language: cxx - c++
# --> allow cmake to find a C++ compiler and set up C++ build rules
project(VM_MONITOR LANGUAGES CXX)

# set the project standard
# compile everything with C++ 23 (must)
# if our compiler doesn't support C++ 23 --> fail (instead of CMake silently fallback to C++ 17)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(branch bench/branch.cpp)
add_executable(chase bench/chase.cpp)
add_executable(l1_assoc bench/l1_assoc.cpp)
add_executable(probe_stability bench/probe_stability.cpp)

add_library(cache_probe daemon/cache_probe.cpp)
target_include_directories(cache_probe PUBLIC daemon)

target_compile_options(branch PRIVATE -O2 -Wall -Wextra)
target_compile_options(chase PRIVATE -O2 -Wall -Wextra)
target_compile_options(l1_assoc PRIVATE -O2 -Wall -Wextra)
target_compile_options(cache_probe PRIVATE -O2 -Wall -Wextra)
target_compile_options(probe_stability PRIVATE -O2 -Wall -Wextra)
target_link_libraries(probe_stability PRIVATE cache_probe)

# run the CMake file:
# cmake -S . -B build
# cmake --build build
# ./build/branch rand-branch 200
