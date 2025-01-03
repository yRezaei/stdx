cmake_minimum_required(VERSION 3.15)
project(stdx_logger CXX)

set(LIBRARY_TYPE STATIC) # Default to STATIC
if (BUILD_SHARED_LIBS)
    set(LIBRARY_TYPE SHARED)
endif()

add_library(stdx_logger ${LIBRARY_TYPE}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/logger.cxx
)

if(BUILD_SHARED_LIBS)
    target_compile_definitions(stdx_logger PRIVATE BUILDING_STDX_LOGGER)
    target_compile_definitions(stdx_logger PUBLIC STDX_SHARED)
else()
    target_compile_definitions(stdx_logger PUBLIC STDX_STATIC)
endif()

target_include_directories(stdx_logger PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Test
add_executable(test_logger tests/test_logger.cxx)
target_link_libraries(test_logger PRIVATE stdx_logger)
add_test(NAME test_logger COMMAND test_logger)

# Install
install(TARGETS stdx_logger
    EXPORT stdx_loggerConfig
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

if(MSVC)
    install(FILES $<TARGET_PDB_FILE:stdx_logger> DESTINATION bin OPTIONAL)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # On Linux or macOS, debug symbols are usually embedded in the binary.
    # If separated, you might need to specify debug symbol files explicitly.
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/stdx_logger.debug DESTINATION lib OPTIONAL)
endif()

install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/stdx/"
    DESTINATION "include/stdx"
)
