cmake_minimum_required(VERSION 3.10)
project(plugin_v1_lru_example)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_C_VISIBILITY_PRESET default)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 0)


# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)
message(STATUS "GLIB library: ${GLIB_LIBRARIES}")
message(STATUS "GLIB include: ${GLIB_INCLUDE_DIRS}")

# Find libCacheSim
# note that we need to find the shared library, not the static library
find_library(LIBCACHESIM_LIBRARY
    NAMES CacheSim libCacheSim_shared
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

# Include directories
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${LIBCACHESIM_INCLUDE_DIR})

# Build plugin LRU shared library (plugin)
add_library(plugin_lru SHARED plugin_lru.c)
target_link_libraries(plugin_lru ${GLIB_LIBRARIES} ${LIBCACHESIM_LIBRARY} m)

# Build test programs
add_executable(test_plugin test_plugin.c)
target_link_libraries(test_plugin
    ${GLIB_LIBRARIES}
    ${LIBCACHESIM_LIBRARY}
    m
    dl)
