cmake_minimum_required(VERSION 3.15)
project(stdx_logger CXX)

# Determine library type (STATIC or SHARED)
set(LIBRARY_TYPE STATIC) # Default to STATIC
if (BUILD_SHARED_LIBS)
    set(LIBRARY_TYPE SHARED)
endif()

# Define the stdx_logger target
add_library(stdx_logger ${LIBRARY_TYPE}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/logger.cxx
)

# Set compile definitions for shared/static builds
if (BUILD_SHARED_LIBS)
    target_compile_definitions(stdx_logger PRIVATE BUILDING_STDX_LOGGER)
    target_compile_definitions(stdx_logger PUBLIC STDX_SHARED)
else()
    target_compile_definitions(stdx_logger PUBLIC STDX_STATIC)
endif()

# Ensure C++17 standard
target_compile_features(stdx_logger PUBLIC cxx_std_17)

# Alias for cleaner target usage
add_library(stdx::logger ALIAS stdx_logger)

# Include directories for the logger module
target_include_directories(stdx_logger PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Unit tests for logger
add_executable(test_logger ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_logger.cxx)
target_link_libraries(test_logger PRIVATE stdx_logger)
add_test(NAME test_logger COMMAND test_logger)

# Installation rules for the library
install(TARGETS stdx_logger
    EXPORT stdx_loggerConfig
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

# Optionally install debug symbols for shared libraries
if (BUILD_SHARED_LIBS)
    if (MSVC)
        install(FILES $<TARGET_PDB_FILE:stdx_logger> DESTINATION bin OPTIONAL)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/stdx_logger.debug DESTINATION lib OPTIONAL)
    endif()
endif()

# Install the export configuration
install(EXPORT stdx_loggerConfig
    FILE stdx_loggerConfig.cmake
    NAMESPACE stdx::
    DESTINATION lib/cmake/stdx_logger
)

# Install header files for the logger module
install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/stdx/"
    DESTINATION "include/stdx"
)

