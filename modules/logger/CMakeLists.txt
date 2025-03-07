
set(_logger_sources
    "${CMAKE_SOURCE_DIR}/src/logger.cxx"
    "${CMAKE_SOURCE_DIR}/src/log_manager.cxx"
    "${CMAKE_SOURCE_DIR}/src/impl/logger_impl.cxx"
)

list(APPEND STDX_SOURCES ${_logger_sources})
set(STDX_SOURCES "${STDX_SOURCES}" PARENT_SCOPE)

list(APPEND STDX_HEADERS
    "${CMAKE_SOURCE_DIR}/include/stdx/logger/logger.hpp"
    "${CMAKE_SOURCE_DIR}/include/stdx/logger/log_manager.hpp"
    "${CMAKE_SOURCE_DIR}/include/stdx/logger/impl/logger_impl.hpp"
    "${CMAKE_SOURCE_DIR}/include/stdx/stdx_export.hpp"
)

install(
    FILES 
        "${CMAKE_SOURCE_DIR}/include/stdx/logger/logger.hpp"
        "${CMAKE_SOURCE_DIR}/include/stdx/logger/log_manager.hpp"
    DESTINATION "include/stdx/logger"
)

install(
    FILES 
        "${CMAKE_SOURCE_DIR}/include/stdx/logger/impl/logger_impl.hpp"
    DESTINATION "include/stdx/logger/impl"
)


# option(STDX_BUILD_TESTS "Enable building tests for the flag module" OFF)
# if(STDX_BUILD_TESTS)
#     find_package(GTest REQUIRED)
#     # Unit tests for logger
#     add_executable(test_logger ${CMAKE_SOURCE_DIR}/test_package/test_logger.cxx)
#     target_link_libraries(test_logger PRIVATE logger gtest::gtest)
#     add_test(NAME test_logger COMMAND test_logger)
# endif()

