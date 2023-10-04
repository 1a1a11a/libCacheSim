cmake_minimum_required (VERSION 3.2)
project (layeredCache)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

message("****************** this example only works after libCacheSim has been installed ******************")


####
## export C_INCLUDE_PATH=/usr/local/include
## export CPLUS_INCLUDE_PATH=/usr/local/include

# clear an error when using cmake in macOS
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(CMAKE_MACOSX_RPATH 1)
    # Fix linking on 10.14+. See https://stackoverflow.com/questions/54068035
    LINK_DIRECTORIES(/usr/local/lib)
    # a temporary fix for mac
    SET(CMAKE_C_FLAGS_DEBUG " -L/usr/local/lib -lglib-2.0 -lintl -Wno-unused-command-line-argument")
endif()


set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
# find libglib2.0-dev
find_package(GLib "2.40" REQUIRED)
include_directories(${GLib_INCLUDE_DIRS})
message(STATUS "glib found? " ${GLib_FOUND} ", GLIB = " ${GLib_LIBRARY} ", header = " ${GLib_INCLUDE_DIRS})

# set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "/usr/local/share/cmake/yaml-cpp/")
find_package(YAML-CPP REQUIRED)
# set(YAML_CPP_INCLUDE_DIR /usr/local/include/)
# set(YAML_CPP_LIBRARIES /usr/local/lib/)
include_directories(${YAML_CPP_INCLUDE_DIR})
message(STATUS ${YAML_CPP_INCLUDE_DIR} ";" ${YAML_CPP_LIBRARIES})


file(GLOB ALL_SRC ${PROJECT_SOURCE_DIR}/*.cpp)
message(STATUS "all sources " ${ALL_SRC})
add_executable(layeredCache ${ALL_SRC})
target_link_libraries(layeredCache yaml-cpp libCacheSim m ${GLib_LIBRARY})






