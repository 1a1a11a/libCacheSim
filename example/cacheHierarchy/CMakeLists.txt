cmake_minimum_required (VERSION 3.10)
project (cacheHierarchy)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED On)
set(CMAKE_CXX_EXTENSIONS Off)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)

# Find ZSTD library
find_library(ZSTD_LIBRARY
    NAMES zstd libzstd
    PATHS /usr/local/lib /usr/lib /opt/homebrew/lib
    DOC "ZSTD library"
)

find_path(ZSTD_INCLUDE_DIR
    NAMES zstd.h
    PATHS /usr/local/include /usr/include /opt/homebrew/include
    DOC "ZSTD include directory"
)

include_directories(${ZSTD_INCLUDE_DIR})
message(STATUS "ZSTD library: ${ZSTD_LIBRARY}")
message(STATUS "ZSTD include: ${ZSTD_INCLUDE_DIR}")

# Find libCacheSim
find_library(LIBCACHESIM_LIBRARY
    NAMES CacheSim libCacheSim
    PATHS /usr/local/lib /usr/lib
    DOC "libCacheSim unified library"
)

find_path(LIBCACHESIM_INCLUDE_DIR
    NAMES libCacheSim.h
    PATHS /usr/local/include /usr/include
    DOC "libCacheSim include directory"
)

message(STATUS "libCacheSim library: ${LIBCACHESIM_LIBRARY}")
message(STATUS "libCacheSim include: ${LIBCACHESIM_INCLUDE_DIR}")

if(NOT LIBCACHESIM_LIBRARY OR NOT LIBCACHESIM_INCLUDE_DIR)
    message(FATAL_ERROR "libCacheSim not found! Please install libCacheSim first.")
endif()

add_executable(cacheHierarchy main.cpp myconfig.cpp simulator.cpp utils.cpp)

target_include_directories(cacheHierarchy PRIVATE
    ${ZSTD_INCLUDE_DIR}
    ${LIBCACHESIM_INCLUDE_DIR}
)

# Add GLib compile and link flags
target_compile_options(cacheHierarchy PRIVATE ${GLIB_CFLAGS_OTHER})
target_include_directories(cacheHierarchy PRIVATE ${GLIB_INCLUDE_DIRS})
target_link_directories(cacheHierarchy PRIVATE ${GLIB_LIBRARY_DIRS})


target_link_libraries(cacheHierarchy
    ${LIBCACHESIM_LIBRARY}
    m
    ${GLIB_LIBRARIES}
    ${ZSTD_LIBRARY}
)
