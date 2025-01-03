cmake_minimum_required(VERSION 3.15)
project(stdx_flag CXX)

add_library(stdx_flag INTERFACE)

target_include_directories(stdx_flag INTERFACE
    # Use CMAKE_SOURCE_DIR to point to the *root* of your project.
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include> 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

add_executable(test_flag tests/test_flag.cxx)
target_link_libraries(test_flag PRIVATE stdx_flag)
add_test(NAME test_flag COMMAND test_flag)

install(TARGETS stdx_flag
    EXPORT stdx_flagConfig
)

install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/stdx/"
    DESTINATION "include/stdx"
)
