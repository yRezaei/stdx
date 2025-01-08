cmake_minimum_required(VERSION 3.15)
project(logger CXX)

# Determine library type (STATIC or SHARED)
set(LIBRARY_TYPE STATIC) # Default to STATIC
if (BUILD_SHARED_LIBS)
    set(LIBRARY_TYPE SHARED)
endif()

# Define the logger target
add_library(logger ${LIBRARY_TYPE}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/logger.cxx
)

# Ensure C++17 standard
target_compile_features(logger PUBLIC cxx_std_17)

# Set compile definitions for shared/static builds
if (BUILD_SHARED_LIBS)
    target_compile_definitions(logger PRIVATE BUILDING_STDX_LOGGER)
    target_compile_definitions(logger PUBLIC STDX_SHARED)
else()
    target_compile_definitions(logger PUBLIC STDX_STATIC)
endif()

# Ensure consumers inherit the correct macro
target_compile_definitions(logger INTERFACE $<TARGET_PROPERTY:logger,COMPILE_DEFINITIONS>)

# Include directories for the logger module
target_include_directories(logger PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Optionally install debug symbols for shared libraries
if (BUILD_SHARED_LIBS)
    if (MSVC)
        install(FILES $<TARGET_PDB_FILE:logger> DESTINATION bin OPTIONAL)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/logger.debug DESTINATION lib OPTIONAL)
    endif()
endif()

# Installation rules for the library
install(TARGETS logger
    EXPORT loggerConfig
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

# Install the export configuration
install(EXPORT loggerConfig
    FILE loggerConfig.cmake
    NAMESPACE stdx::
    DESTINATION cmake
)

# Install header files for the logger module
install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/stdx/"
    DESTINATION "include/stdx"
)

option(STDX_BUILD_TESTS "Enable building tests for the flag module" OFF)
if(STDX_BUILD_TESTS)
    find_package(GTest REQUIRED)
    # Unit tests for logger
    add_executable(test_logger ${CMAKE_SOURCE_DIR}/test_package/test_logger.cxx)
    target_link_libraries(test_logger PRIVATE logger gtest::gtest)
    add_test(NAME test_logger COMMAND test_logger)
endif()

