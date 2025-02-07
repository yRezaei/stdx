cmake_minimum_required(VERSION 3.15)
project(test_package CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(GTest REQUIRED)

# Flag test
find_package(stdx REQUIRED)
add_executable(test_stdx test_stdx.cxx)
target_link_libraries(test_stdx PUBLIC stdx::stdx gtest::gtest)


# Flag test
if(STDX_ENABLE_FLAG)
    find_package(stdx REQUIRED)
    add_executable(test_flag test_flag.cxx)
    target_link_libraries(test_flag PUBLIC stdx::stdx gtest::gtest)
endif()

# Logger test
if(STDX_ENABLE_LOGGER)
    find_package(stdx REQUIRED)
    add_executable(test_logger test_logger.cxx)
    target_link_libraries(test_logger PUBLIC stdx::stdx gtest::gtest)
endif()


