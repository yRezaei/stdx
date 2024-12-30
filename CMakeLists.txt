cmake_minimum_required(VERSION 3.10)
project(stdx LANGUAGES CXX)

# Project configuration
project(stdx_library VERSION 0.1 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Where to place built binaries (exes, libs) before install:
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Option to build libraries as shared or static
option(BUILD_SHARED_LIBS "Build libraries as shared" OFF)

# Optionally build the 'flag' module
option(BUILD_FLAG_MODULE "Build the Flag module" ON)
option(BUILD_DUMMY_MODULE "Build the Dummy module" ON)

# Enable CTest across the project
enable_testing()

# Include subdirectories for modules
if (BUILD_FLAG_MODULE)
    add_subdirectory(flag)
endif()

if (BUILD_DUMMY_MODULE)
    add_subdirectory(dummy)
endif()

# Install headers to the correct location
install(DIRECTORY include/stdx DESTINATION include)
