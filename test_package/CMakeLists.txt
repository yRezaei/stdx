cmake_minimum_required(VERSION 3.15)
project(test_package CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(GTest REQUIRED)

# Flag test
find_package(stdx REQUIRED)

# Flag test
if(STDX_ENABLE_FLAG)
    add_executable(test_flag test_flag.cxx)
    target_link_libraries(test_flag PUBLIC stdx::stdx gtest::gtest)
endif()

# Function ref test
add_executable(test_function_ref test_function_ref.cxx)
target_link_libraries(test_function_ref PUBLIC stdx::stdx gtest::gtest)

# Light callable test
add_executable(test_light_callable test_light_callable.cxx)
target_link_libraries(test_light_callable PUBLIC stdx::stdx gtest::gtest)

# Singleton test
add_executable(test_singleton test_singleton.cxx)
target_link_libraries(test_singleton PUBLIC stdx::stdx gtest::gtest)

# Ring buffer test
add_executable(test_ring_buffer test_ring_buffer.cxx)
target_link_libraries(test_ring_buffer PUBLIC stdx::stdx gtest::gtest)

# Thread pool test
add_executable(test_thread_pool test_thread_pool.cxx)
target_link_libraries(test_thread_pool PUBLIC stdx::stdx gtest::gtest)

# Timer test
add_executable(test_timer test_timer.cxx)
target_link_libraries(test_timer PUBLIC stdx::stdx gtest::gtest)

# Logger test
if(STDX_ENABLE_LOGGER)
    add_executable(test_logger test_logger.cxx)
    target_link_libraries(test_logger PUBLIC stdx::stdx gtest::gtest)
endif()


