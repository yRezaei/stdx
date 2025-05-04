list(APPEND STDX_HEADERS
    "${CMAKE_SOURCE_DIR}/include/stdx/light_callable.hpp"
)

install(
    FILES "${CMAKE_SOURCE_DIR}/include/stdx/light_callable.hpp"
    DESTINATION "include/stdx"
)