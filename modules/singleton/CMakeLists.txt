list(APPEND STDX_HEADERS
    "${CMAKE_SOURCE_DIR}/include/stdx/singleton.hpp"
)

install(
    FILES "${CMAKE_SOURCE_DIR}/include/stdx/singleton.hpp"
    DESTINATION "include/stdx"
)