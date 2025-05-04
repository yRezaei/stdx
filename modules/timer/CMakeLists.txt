set(_sources
    "${CMAKE_SOURCE_DIR}/src/timer.cxx"
)

list(APPEND STDX_SOURCES ${_sources})
set(STDX_SOURCES "${STDX_SOURCES}" PARENT_SCOPE)

list(APPEND STDX_HEADERS
    "${CMAKE_SOURCE_DIR}/include/stdx/threading/timer.hpp"
    "${CMAKE_SOURCE_DIR}/include/stdx/threading/thread_pool.hpp"
    "${CMAKE_SOURCE_DIR}/include/stdx/stdx_export.hpp"
)

install(
    FILES 
        "${CMAKE_SOURCE_DIR}/include/stdx/threading/timer.hpp"
    DESTINATION "include/stdx/threading"
)