
list(APPEND STDX_HEADERS
    "${CMAKE_SOURCE_DIR}/include/stdx/concurrency/locked_queue.hpp"
)

install(
    FILES 
        "${CMAKE_SOURCE_DIR}/include/stdx/concurrency/locked_queue.hpp"
    DESTINATION "include/stdx/concurrency"
)

# option(STDX_BUILD_TESTS "Enable building tests for the flag module" OFF)
# if(STDX_BUILD_TESTS)
#     find_package(GTest REQUIRED)
#     # Add the test executable
#     add_executable(test_flag ${CMAKE_SOURCE_DIR}/test_package/test_flag.cxx)
#     target_link_libraries(test_flag PRIVATE flag gtest::gtest)
    
#     # Add the test to CTest
#     add_test(NAME test_flag COMMAND test_flag)
# endif()
