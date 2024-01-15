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


# https://github.com/jbeder/yaml-cpp
# wget https://github.com/jbeder/yaml-cpp/archive/refs/tags/0.8.0.tar.gz
# tar -xzf 0.8.0.tar.gz; mkdir yaml-cpp-0.8.0/_build; cd yaml-cpp-0.8.0/_build; 
# cmake -DBUILD_SHARED_LIBS=ON ..; 
# make -j; sudo make install
find_package(YAML-CPP REQUIRED)
# set(YAML_CPP_INCLUDE_DIR /usr/local/include/)
# set(YAML_CPP_LIBRARIES /usr/local/lib/)
include_directories(${YAML_CPP_INCLUDE_DIR})
message(STATUS ${YAML_CPP_INCLUDE_DIR} ";" ${YAML_CPP_LIBRARIES})


file(GLOB ALL_SRC ${PROJECT_SOURCE_DIR}/*.cpp)
message(STATUS "all sources " ${ALL_SRC})
add_executable(layeredCache ${ALL_SRC})
target_link_libraries(layeredCache yaml-cpp libCacheSim dl m ${GLib_LIBRARY} ${ZSTD_LIBRARIES} pthread)

