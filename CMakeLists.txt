cmake_minimum_required(VERSION 3.2)
project(libCacheSim C)
set(DESCRIPTION "a high performance cache simulation library")
set(PROJECT_WEB "http://libCacheSim.info")

set(${PROJECT_NAME}_VERSION_MAJOR 0)
set(${PROJECT_NAME}_VERSION_MINOR 1)
set(${PROJECT_NAME}_VERSION_REVISION 0)
set(${PROJECT_NAME}_RELEASE_VERSION ${${PROJECT_NAME}_VERSION_MAJOR}.${${PROJECT_NAME}_VERSION_MINOR})
set(${PROJECT_NAME}_VERSION ${${PROJECT_NAME}_RELEASE_VERSION}.${${PROJECT_NAME}_VERSION_REVISION})


####################
# detect platform #
####################
macro(set_platform system_name)
    if (${system_name} MATCHES "Darwin")
        set(OS_PLATFORM "OS_DARWIN")
        add_definitions(-DOS_DARWIN)

        set(CMAKE_BUILD_TYPE Debug)
        set(CMAKE_MACOSX_RPATH 1)
        # Fix linking on 10.14+. See https://stackoverflow.com/questions/54068035
        LINK_DIRECTORIES(/usr/local/lib)
        # a temporary fix for mac
        SET(CMAKE_C_FLAGS "-Wno-unused-command-line-argument -lintl -L/usr/local/lib -lglib-2.0 -lintl -I/usr/local/Cellar/gettext/0.20.1/include/")
        SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I/usr/local/Cellar/python/3.7.5/Frameworks/Python.framework/Versions/3.7/include/python3.7m/")
        SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I/Users/jason/Library/Python/3.7/lib/python/site-packages/numpy/core/include/")

    elseif (${system_name} MATCHES "Linux")
        set(OS_PLATFORM "OS_LINUX")
        add_definitions(-DOS_LINUX)
    else ()
        set(OS_PLATFORM "OS_UNSUPPORTED")
        message(FATAL_ERROR "unsupported operating system")
    endif ()
endmacro(set_platform)

set_platform(${CMAKE_SYSTEM_NAME})
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()


####################
# define options #
####################
option(SUPPORT_HUGEPAGE "use transparent huge page, can provide % improvement, do `echo madvise | sudo tee /sys/kernel/mm/transparent_hugepage/enabled`" ON)
option(ENABLE_TESTS "whether enable test" ON)


# set compiler flags
# string concat is easier in 3.0, but older versions don't have the concat subcommand
# so we are using list as input until we move to new version
# add_definitions(-D_GNU_SOURCE -D_FILE_OFFSET_BITS=64)


####################
# define variables #
####################
set(CFLAGS "$ENV{CFLAGS} "
        "-std=gnu99 ${CMAKE_ENABLE_C99} "
        "-Wall -Wshadow -Winline "
        "-Wno-unused "
        #        "-Wstrict-prototypes -Wmissing-prototypes "
        #        "-Wmissing-declarations "
        "-Wredundant-decls "
        #        "-Wunused-function -Wunused-value -Wunused-variable "
        #    -std=c11     "-fno-strict-aliasing "
        )


string(REPLACE "" "" LOCAL_CFLAGS ${CFLAGS})
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} $ENV{CFLAGS} ${LOCAL_CFLAGS}")


if (CMAKE_BUILD_TYPE MATCHES Debug)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g -ggdb3 -DLOGLEVEL=5 ")
else ()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -DLOGLEVEL=7")
endif ()


####################
# find dependency #
####################
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

find_package(GLib REQUIRED)
include_directories(${GLib_INCLUDE_DIRS})
set(LIBS ${LIBS} ${GLib_LIBRARY})

find_package(Tcmalloc REQUIRED)
set(LIBS ${LIBS} ${Tcmalloc_LIBRARIES})
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free")

#find_package(xxHash)
#include_directories(${xxHash_INCLUDE_DIR})
#set(LIBS ${LIBS} ${xxHash_LIBRARIES})
#message(STATUS "xxHash found? " ${xxHash_FOUND} " LIBS = " ${xxHash_LIBRARIES} ", header = " ${xxHash_INCLUDE_DIRS})
#if(NOT xxHash_INCLUDE_DIR)
#    message(STATUS "#### WARNING ####: xxhash library not found, use murmur3")
#    add_compile_definitions(HASH_TYPE=0xb10)
#else()
##    add_compile_definitions(HASH_TYPE=0xb10)
#    add_compile_definitions(HASH_TYPE=0xb21)
#endif()


message(STATUS "<<++=====------------------\\/------------------=====++>>")
message(STATUS "<<++              libCacheSim summary             ++>>")
message(STATUS "<<++=====------------------/\\------------------=====++>>")

message(STATUS "================== dependency related ==================")
message(STATUS "glib found?     " ${GLib_FOUND} " - LIBS = " ${GLib_LIBRARY} ", header = " ${GLib_INCLUDE_DIRS})
message(STATUS "Tcmalloc found? " ${Tcmalloc_FOUND} " - LIBS = " ${Tcmalloc_LIBRARIES} ", header = " ${Tcmalloc_INCLUDE_DIRS})

message(STATUS "==================== CMake related =====================")
message(STATUS "platform          = ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_VERSION}")
message(STATUS "CPU type          = ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "cmake source      = ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "cmake compiler    = ${CMAKE_C_COMPILER}")
message(STATUS "CMAKE_BUILD_TYPE  = ${CMAKE_BUILD_TYPE}")
message(STATUS "CFLAGS            = ${CMAKE_C_FLAGS}")
message(STATUS "LIBS              = ${LIBS}")
message(STATUS "Installation path = ${CMAKE_INSTALL_PREFIX}")

message(STATUS "========================================================")

# message(STATUS "=======Status of system features=======")
# message(STATUS "HAVE_BACKTRACE: " ${HAVE_BACKTRACE})
# message(STATUS "=======================================")

message(STATUS "============= Status of optional features ==============")
# message(STATUS "HAVE_RUST: " ${HAVE_RUST})
# message(STATUS "HAVE_TEST: " ${HAVE_TEST})
message(STATUS "========================================================")



add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libCacheSim)

configure_file(libCacheSim.pc.in libCacheSim.pc @ONLY)
#set(ConfigPackageLocation "lib${LIB_SUFFIX}/cmake/${PROJECT_NAME}")
#install(FILES ${PROJECT_NAME}Config.cmake ${PROJECT_NAME}ConfigVersion.cmake
#        DESTINATION "${ConfigPackageLocation}"
#        COMPONENT Devel
#        )

####################
# library install  #
####################
file(GLOB ALL_SRC_FILES ${PROJECT_SOURCE_DIR}/libCacheSim/*/*.c)
add_library(${PROJECT_NAME} SHARED ".")
target_link_libraries(${PROJECT_NAME} cache cacheAlgo traceReader profiler dataStructure traceStat utils m ${LIBS} ${CMAKE_THREAD_LIBS_INIT} -lpthread -ldl)
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${${PROJECT_NAME}_VERSION})
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE C)
#message(STATUS "PROJECT_SOURCE_DIR ${PROJECT_SOURCE_DIR}")
#target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/libCacheSim/include)

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION lib)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/libCacheSim/include/ DESTINATION include)

#install(TARGETS ${PROJECT_NAME}
#        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
#        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ${CMAKE_BINARY_DIR}/libCacheSim.pc DESTINATION lib/pkgconfig)

#install(TARGETS ${PROJECT_NAME}
#        # IMPORTANT: Add the mylib library to the "export-set"
#        EXPORT ${PROJECT_NAME}
#        LIBRARY DESTINATION "${INSTALL_LIB_DIR}"
#        PUBLIC_HEADER DESTINATION "${INSTALL_INCLUDE_DIR}"
#        COMPONENT dev)

#target_compile_definitions(binBenchmark PRIVATE HASH=0xb2)
add_executable(binBenchmark libCacheSim/benchmark/main.c)
target_link_libraries(binBenchmark benchmark cache cacheAlgo traceReader profiler dataStructure traceStat utils m ${LIBS} ${CMAKE_THREAD_LIBS_INIT} -lpthread -ldl)

#add_executable(binDebug ../test/debug.c)
#target_link_libraries(binDebug cache cacheAlgo traceReader profiler dataStructure traceStat utils m ${LIBS} ${CMAKE_THREAD_LIBS_INIT} -lpthread -ldl)
#set_property(TARGET binDebug PROPERTY C_STANDARD 11)


## Add all targets to the build-tree export set
#export(TARGETS libCacheSim
#        FILE "${PROJECT_BINARY_DIR}/libCacheSim.cmake")
#
## Export the package for use from the build-tree
## (this registers the build-tree with a global CMake-registry)
#export(PACKAGE libCacheSim)

## Create the FooBarConfig.cmake and FooBarConfigVersion files
#file(RELATIVE_PATH REL_INCLUDE_DIR "${INSTALL_CMAKE_DIR}"
#        "${INSTALL_INCLUDE_DIR}")
## ... for the build tree
#set(CONF_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}" "${PROJECT_BINARY_DIR}")
#configure_file(libCacheSim.cmake.in
#        "${PROJECT_BINARY_DIR}/libCacheSim.cmake" @ONLY)
## ... for the install tree
#set(CONF_INCLUDE_DIRS "\${FOOBAR_CMAKE_DIR}/${REL_INCLUDE_DIR}")
#configure_file(libCacheSim.cmake.in
#        "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/libCacheSim.cmake" @ONLY)
#
#
## Install the FooBarConfig.cmake and FooBarConfigVersion.cmake
#install(FILES
#        "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/libCacheSim.cmake"
#        DESTINATION "${INSTALL_CMAKE_DIR}" COMPONENT dev)
#
## Install the export set for use with the install-tree
#install(EXPORT libCacheSim DESTINATION
#        "${INSTALL_CMAKE_DIR}" COMPONENT dev)


if (ENABLE_TESTS)
    include(CTest)
    enable_testing()
    message(STATUS "Building with test")
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/test)
else ()
    message(STATUS "Building without test")
endif ()




# Example Usages:
# mkdir _build;
# cd _build && rm -r *;
# cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON .. && make -j
# ctest

