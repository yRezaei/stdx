cmake_minimum_required(VERSION 3.15)
project(stdx LANGUAGES CXX)

# ------------------------------------------------------------------------------
# 1) Options for modules
# ------------------------------------------------------------------------------
option(STDX_ENABLE_FLAG             "Enable 'flag' module"              ON)
option(STDX_ENABLE_LOCKED_QUEUE     "Enable 'locked queue' module"      ON)
option(STDX_ENABLE_RING_BUFFER      "Enable 'ring buffer' module"       ON)
option(STDX_ENABLE_THREAD_POOL      "Enable 'thread pool' module"       ON)
option(STDX_ENABLE_LOGGER           "Enable 'logger' module"            ON)


set(STDX_SOURCES "")       # Will contain .cxx/.cpp
set(STDX_DEFINES "")       # For compile_definitions if needed
set(STDX_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/include")  # For include_directories


if(STDX_ENABLE_FLAG)
    add_subdirectory(modules/flag)
endif()

if(STDX_ENABLE_LOCKED_QUEUE)
    add_subdirectory(modules/locked_queue)
endif()

if(STDX_ENABLE_RING_BUFFER)
    add_subdirectory(modules/ring_buffer)
endif()

if(STDX_ENABLE_THREAD_POOL)
    add_subdirectory(modules/thread_pool)
endif()

if(STDX_ENABLE_LOGGER)
    add_subdirectory(modules/logger)
endif()

# ------------------------------------------------------------------------------
# 5) Create the "stdx" library
#    - If we have any .cpp files, compile a normal library.
#    - If no sources, it's effectively header-only (INTERFACE library).
# ------------------------------------------------------------------------------

if(STDX_SOURCES)
    if(BUILD_SHARED_LIBS)
        add_library(stdx SHARED ${STDX_SOURCES})
        target_compile_definitions(stdx PRIVATE STDX_BUILD_SHARED STDX_INCLUDE_EXPORT)
        target_compile_definitions(stdx INTERFACE STDX_USE_SHARED)
    else()
        add_library(stdx STATIC ${STDX_SOURCES})
        target_compile_definitions(stdx PRIVATE STDX_INCLUDE_EXPORT)
    endif()

    include_directories(${STDX_INCLUDE_DIRS})
    target_compile_features(stdx PUBLIC cxx_std_17)
else()
    message(STATUS "No source files; building stdx as a header-only (INTERFACE) library.")
    add_library(stdx INTERFACE)
    include_directories(${STDX_INCLUDE_DIRS})
    target_compile_features(stdx INTERFACE cxx_std_17)
endif()

# ------------------------------------------------------------------------------
# 6) Installation of library and headers
# ------------------------------------------------------------------------------
install(TARGETS stdx
    EXPORT stdxConfig
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(EXPORT stdxConfig
    FILE stdxConfig.cmake
    NAMESPACE stdx::
    DESTINATION cmake
)

if (BUILD_SHARED_LIBS)
    if (MSVC)
        install(FILES $<TARGET_PDB_FILE:stdx> DESTINATION bin OPTIONAL)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/stdx.debug DESTINATION lib OPTIONAL)
    endif()
endif()
