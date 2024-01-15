cmake_minimum_required (VERSION 3.2)
project (cacheSimulator)
# set(CMAKE_BUILD_TYPE Debug)

message("****************** this example only works after libCacheSim has been installed ******************")

find_library(libCacheSim libCacheSim)
find_path(libCacheSimInclude libCacheSim)
message(STATUS "libCacheSim found? ${libCacheSim} ${libCacheSimInclude}")
include_directories(${libCacheSimInclude})

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/../../cmake/Modules/")
find_package(GLib "2.40" REQUIRED)
include_directories(${GLib_INCLUDE_DIRS})
message(STATUS "glib found? ${GLib_FOUND}, lib = ${GLib_LIBRARY}, header = ${GLib_INCLUDE_DIRS}")

find_package(ZSTD)
# https://stackoverflow.com/questions/61377055/cannot-find-gflags-gflags-h-while-building-library-osx/61379123#61379123
include_directories(${ZSTD_INCLUDE_DIR})
if ("${ZSTD_LIBRARIES}" STREQUAL "") 
    message(FATAL_ERROR "zstd not found")
endif()


add_executable(cacheSimulator main.c)
target_link_libraries(cacheSimulator libCacheSim m ${GLib_LIBRARY} ${ZSTD_LIBRARIES})
