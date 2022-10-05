cmake_minimum_required (VERSION 2.8)
project (cacheSimulator)
# set(CMAKE_BUILD_TYPE Debug)

message("****************** this example only works after libCacheSim has been installed ******************")


# clear an error when using cmake in macOS
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(CMAKE_MACOSX_RPATH 1)
    # Fix linking on 10.14+. See https://stackoverflow.com/questions/54068035
    LINK_DIRECTORIES(/usr/local/lib)
    # a temporary fix for mac
    SET(CMAKE_C_FLAGS_DEBUG " -L/usr/local/lib -lglib-2.0 -lintl -Wno-unused-command-line-argument")
endif()

find_library(libCacheSim libCacheSim)
find_path(libCacheSimInclude libCacheSim)
message(STATUS "libCacheSim found? ${libCacheSim} ${libCacheSimInclude}")
include_directories(${libCacheSimInclude})

# find_package (libCacheSim)
# message(STATUS "libCacheSim found? ${libCacheSim_FOUND}, ${libCacheSim_INCLUDE_DIRS}, ${libCacheSim_LIBRARIES}")
# if (libCacheSim_FOUND)
#    include_directories(${libCacheSim_INCLUDE_DIRS})
#    target_link_libraries (helloworld ${libCacheSim_LIBRARIES})
# endif (libCacheSim_FOUND)


set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
find_package(GLib "2.40" REQUIRED)
include_directories(${GLib_INCLUDE_DIRS})
message(STATUS "glib found? ${GLib_FOUND}, lib = ${GLib_LIBRARY}, header = ${GLib_INCLUDE_DIRS}")


add_executable(cacheSimulator main.c)
target_link_libraries(cacheSimulator libCacheSim m ${GLib_LIBRARY} -lpthread -ldl)
