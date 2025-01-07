cmake_minimum_required(VERSION 3.15)
project(flag CXX)

add_library(flag INTERFACE)

# Include directories for the flag module
target_include_directories(flag INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include> 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Ensure consumers inherit the correct macro
target_compile_definitions(flag INTERFACE $<TARGET_PROPERTY:flag,COMPILE_DEFINITIONS>)

# Install the export configuration
install(TARGETS flag
    EXPORT flagConfig
    INCLUDES DESTINATION include
)

install(EXPORT flagConfig
    FILE flagConfig.cmake
    NAMESPACE stdx::
    DESTINATION cmake
)

install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/stdx/"
    DESTINATION "include/stdx"
)

option(STDX_BUILD_TESTS "Enable building tests for the flag module" OFF)
if(STDX_BUILD_TESTS)
    find_package(GTest REQUIRED)
    # Add the test executable
    add_executable(test_flag ${CMAKE_SOURCE_DIR}/tests/test_flag.cxx)
    target_link_libraries(test_flag PRIVATE flag gtest::gtest)
    
    # Add the test to CTest
    add_test(NAME test_flag COMMAND test_flag)
endif()
