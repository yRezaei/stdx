# flag/CMakeLists.txt

# Define the library. Since this is header-only, it remains INTERFACE.
add_library(stdx_flag INTERFACE)

# Include directories for build and installation
target_include_directories(stdx_flag
    INTERFACE
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Install instructions for the library and headers into the root/bin folder
install(TARGETS stdx_flag
    EXPORT stdx_targets
    RUNTIME DESTINATION .
    LIBRARY DESTINATION .
    ARCHIVE DESTINATION .
)

install(
    DIRECTORY "${PROJECT_SOURCE_DIR}/include/stdx"
    DESTINATION include
)

# Tests for the Flag module
option(BUILD_FLAG_TESTS "Build tests for the Flag module" ON)

if (BUILD_FLAG_TESTS)
    # Create a test executable
    add_executable(test_flag
        test/flag_tests.cxx
    )

    # Link your flag library
    target_link_libraries(test_flag PRIVATE stdx_flag)

    # Register this test with CTest
    add_test(NAME test_flag COMMAND test_flag)
endif()
