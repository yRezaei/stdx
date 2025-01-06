cmake_minimum_required(VERSION 3.15)
project(test_package CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add the package dependencies
find_package(stdx REQUIRED)

# Logger test
if(STDX_ENABLE_LOGGER)
    add_executable(test_logger src/test_logger.cxx)
    target_include_directories(test_logger PRIVATE ${stdx_INCLUDE_DIRS})
    target_link_libraries(test_logger stdx::stdx)
endif()

# Flag test
if(STDX_ENABLE_FLAG)
    add_executable(test_flag src/test_flag.cxx)
    target_include_directories(test_flag PRIVATE ${stdx_INCLUDE_DIRS})
endif()
