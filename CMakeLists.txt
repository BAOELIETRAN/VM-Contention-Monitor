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
