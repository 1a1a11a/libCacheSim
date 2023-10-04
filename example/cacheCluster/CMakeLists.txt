cmake_minimum_required (VERSION 3.2)
project (cacheCluster)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# clear an error when using cmake in macOS
# if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
#     set(CMAKE_MACOSX_RPATH 1)
#     # Fix linking on 10.14+. See https://stackoverflow.com/questions/54068035
#     LINK_DIRECTORIES(/usr/local/lib)
#     # a temporary fix for mac
#     SET(CMAKE_C_FLAGS_DEBUG " -L/usr/local/lib -lglib-2.0 -lintl -Wno-unused-command-line-argument")
# endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/../../cmake/Modules/")
find_package(GLib "2.40" REQUIRED)
include_directories(${GLib_INCLUDE_DIRS})
message(STATUS "glib found? " ${GLib_FOUND} ", GLIB = " ${GLib_LIBRARY} ", header = " ${GLib_INCLUDE_DIRS})

################ this mode compiles the external cache replacement algorithm together with the simulator ############
message(STATUS "project_source dir = " ${PROJECT_SOURCE_DIR})
file(GLOB ALL_CXX_SRC ${PROJECT_SOURCE_DIR}/*.cpp)
file(GLOB ALL_C_SRC ${PROJECT_SOURCE_DIR}/*.c)

add_executable(cacheCluster ${ALL_CXX_SRC} ${ALL_C_SRC})
target_link_libraries(cacheCluster libCacheSim m ${GLib_LIBRARY} pthread dl)

