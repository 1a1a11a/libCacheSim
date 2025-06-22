cmake_minimum_required(VERSION 3.10)
# Collect source files
project(cacheCluster)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Find required packages
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/../../cmake/Modules/")

find_package(GLib "2.40" REQUIRED)
message(STATUS "GLib found: ${GLib_LIBRARY}")

find_package(ZSTD REQUIRED)
if("${ZSTD_LIBRARIES}" STREQUAL "")
    message(FATAL_ERROR "ZSTD not found")
endif()


################ this mode compiles the external cache replacement algorithm together with the simulator ############
message(STATUS "project_source dir = " ${PROJECT_SOURCE_DIR})
file(GLOB ALL_CXX_SRC ${PROJECT_SOURCE_DIR}/*.cpp)
file(GLOB ALL_C_SRC ${PROJECT_SOURCE_DIR}/*.c)
target_include_directories(cacheCluster PRIVATE
    ${GLib_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/include
)

add_executable(cacheCluster ${ALL_CXX_SRC} ${ALL_C_SRC})
target_link_libraries(cacheCluster libCacheSim m ${GLib_LIBRARY} pthread dl ${ZSTD_LIBRARIES})
