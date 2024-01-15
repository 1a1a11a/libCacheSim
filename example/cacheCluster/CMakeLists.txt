cmake_minimum_required (VERSION 3.2)
project (cacheCluster)
# set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/../../cmake/Modules/")
find_package(GLib "2.40" REQUIRED)
include_directories(${GLib_INCLUDE_DIRS})
message(STATUS "glib found? " ${GLib_FOUND} ", GLIB = " ${GLib_LIBRARY} ", header = " ${GLib_INCLUDE_DIRS})


find_package(ZSTD)
# https://stackoverflow.com/questions/61377055/cannot-find-gflags-gflags-h-while-building-library-osx/61379123#61379123
include_directories(${ZSTD_INCLUDE_DIR})
if ("${ZSTD_LIBRARIES}" STREQUAL "") 
    message(FATAL_ERROR "zstd not found")
endif()


################ this mode compiles the external cache replacement algorithm together with the simulator ############
message(STATUS "project_source dir = " ${PROJECT_SOURCE_DIR})
file(GLOB ALL_CXX_SRC ${PROJECT_SOURCE_DIR}/*.cpp)
file(GLOB ALL_C_SRC ${PROJECT_SOURCE_DIR}/*.c)

add_executable(cacheCluster ${ALL_CXX_SRC} ${ALL_C_SRC})
target_link_libraries(cacheCluster libCacheSim m ${GLib_LIBRARY} pthread dl ${ZSTD_LIBRARIES})

