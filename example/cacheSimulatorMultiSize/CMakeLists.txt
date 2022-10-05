cmake_minimum_required (VERSION 2.8)
project (cacheSimulatorMultiSize)
# set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

find_library(libCacheSim libCacheSim)
find_path(libCacheSimInclude libCacheSim)
include_directories(${libCacheSimInclude})
message(STATUS "libCacheSim found? ${libCacheSim}, ${libCacheSimInclude}")

find_package(GLib "2.40" REQUIRED)
include_directories(${GLib_INCLUDE_DIRS})
message(STATUS "glib found? ${GLib_FOUND}, lib = ${GLib_LIBRARY}, header = ${GLib_INCLUDE_DIRS}")


add_executable(cacheSimulatorMultiSize main.cpp)
target_link_libraries(cacheSimulatorMultiSize libCacheSim)
