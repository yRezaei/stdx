# dummy/CMakeLists.txt

# Define the library
add_library(stdx_dummy)

# Add the source file for the dummy module
target_sources(stdx_dummy PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/dummy.cxx
)

# Include directories for the library
target_include_directories(stdx_dummy
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        $<INSTALL_INTERFACE:include>
)

# Set properties for shared/static library builds
set_target_properties(stdx_dummy PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Install the library
install(TARGETS stdx_dummy
    EXPORT stdx_targets
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../include/stdx
    DESTINATION include
)
