cmake_minimum_required(VERSION 3.15)
project(test_package CXX)

# Add the package dependencies
find_package(stdx REQUIRED)

# Logger test
if(STDX_ENABLE_LOGGER)
    add_executable(test_logger test_logger.cpp)
    target_link_libraries(test_logger stdx::stdx_logger)
endif()

# Flag test
if(STDX_ENABLE_FLAG)
    add_executable(test_flag test_flag.cpp)
    target_include_directories(test_flag PRIVATE ${stdx_INCLUDE_DIRS})
endif()
