cmake_minimum_required(VERSION 3.12)

# Set the project name
project(plugin_lru_example)


# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable position independent code for shared libraries
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Find required dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)
message(STATUS "GLIB library: ${GLIB_LIBRARIES}")
message(STATUS "GLIB include: ${GLIB_INCLUDE_DIRS}")

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

# Add the shared library target
add_library(plugin_lru_hooks SHARED plugin_lru.cpp)

# Set compiler flags
target_compile_options(plugin_lru_hooks PRIVATE
    -Wall -Wextra 
    -Wno-unused-variable -Wno-unused-function -Wno-unused-parameter
)
target_include_directories(plugin_lru_hooks PRIVATE
    ${GLIB_INCLUDE_DIRS}
    ${LIBCACHESIM_INCLUDE_DIR}
)
# Link libraries
target_link_libraries(plugin_lru_hooks
    ${GLIB_LIBRARIES}
    ${LIBCACHESIM_LIBRARY}
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_hooks_plugin.c")
    add_executable(test_hooks_plugin test_hooks_plugin.c)
    target_include_directories(test_hooks_plugin PRIVATE
        ${LIBCACHESIM_INCLUDE_DIR}
        ${GLIB_INCLUDE_DIRS}
    )
    target_link_libraries(test_hooks_plugin
        ${GLIB_LIBRARIES}
        ${LIBCACHESIM_LIBRARY}
        dl
        m
    )
endif()
