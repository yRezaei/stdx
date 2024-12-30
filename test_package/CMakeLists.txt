cmake_minimum_required(VERSION 3.15)
project(test_package LANGUAGES CXX)

# Include the Conan toolchain
include("${CMAKE_BINARY_DIR}/generators/conan_toolchain.cmake")

# Add the test executable
add_executable(test_package test_package.cxx)

# Use find_package to locate the stdx package
find_package(stdx CONFIG REQUIRED)

# Link the stdx interface library (no linking required, just include paths)
target_link_libraries(test_package stdx::stdx)
