cmake_minimum_required (VERSION 3.2)
project (layeredCache)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

message("****************** this example only works after libCacheSim has been installed ******************")


set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/../../cmake/Modules/")
find_package(GLib "2.40" REQUIRED)
include_directories(${GLib_INCLUDE_DIRS})
message(STATUS "glib found? " ${GLib_FOUND} ", GLIB = " ${GLib_LIBRARY} ", header = " ${GLib_INCLUDE_DIRS})

find_package(ZSTD)
include_directories(${ZSTD_INCLUDE_DIR})
if ("${ZSTD_LIBRARIES}" STREQUAL "") 
    message(FATAL_ERROR "zstd not found")
endif()



file(GLOB ALL_SRC ${PROJECT_SOURCE_DIR}/*.cpp)
message(STATUS "all sources " ${ALL_SRC})
add_executable(layeredCache ${ALL_SRC})
target_link_libraries(layeredCache 
                    -Wl,--whole-archive 
                    libCacheSim 
                    -Wl,--no-whole-archive
                    dl m ${GLib_LIBRARY} ${ZSTD_LIBRARIES} pthread)

