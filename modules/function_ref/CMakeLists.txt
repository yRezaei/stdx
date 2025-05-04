list(APPEND STDX_HEADERS
    "${CMAKE_SOURCE_DIR}/include/stdx/function_ref.hpp"
)

install(
    FILES "${CMAKE_SOURCE_DIR}/include/stdx/function_ref.hpp"
    DESTINATION "include/stdx"
)