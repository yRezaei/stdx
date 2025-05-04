set(_sources
    "${CMAKE_SOURCE_DIR}/src/logger.cxx"
)

list(APPEND STDX_SOURCES ${_sources})
set(STDX_SOURCES "${STDX_SOURCES}" PARENT_SCOPE)

list(APPEND STDX_HEADERS
    "${CMAKE_SOURCE_DIR}/include/stdx/logging/logger.hpp"
)

install(
    FILES 
        "${CMAKE_SOURCE_DIR}/include/stdx/logging/logger.hpp"

    DESTINATION "include/stdx/logging"
)

