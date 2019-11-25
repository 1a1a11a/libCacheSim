cmake_minimum_required (VERSION 2.8)
project (mimircacheExampleC)
set(CMAKE_BUILD_TYPE Debug)

message("****************** this example only works after mimircache has been installed ******************")


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


################ this mode compiles the external cache replacement algorithm together with the simulator ############
#message(STATUS "project_source dir = " ${PROJECT_SOURCE_DIR})
file(GLOB ALL_SRC ${PROJECT_SOURCE_DIR}/*.cpp)
message(STATUS "all sources " ${ALL_SRC})
add_executable(cacheSimulator ${ALL_SRC})
target_link_libraries(cacheSimulator mimircache m ${GLib_LIBRARY} -lpthread -ldl)


################ this mode compiles the external cache replacement algorithm as shared library ################
################ and then loaded by the simulator, note that the shared library must be in the same dir as simulator ############
#add_library(myLRU SHARED ${PROJECT_SOURCE_DIR}/myLRU.cpp)
#set_target_properties(myLRU PROPERTIES LINKER_LANGUAGE C)

#add_executable(cacheSimulator ${PROJECT_SOURCE_DIR}/cacheSimulator.cpp)
#target_link_libraries(cacheSimulator mimircache m ${GLib_LIBRARY} -lpthread -ldl)

