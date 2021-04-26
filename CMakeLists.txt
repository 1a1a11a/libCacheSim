cmake_minimum_required(VERSION 3.12)
project(libCacheSim C)
set(DESCRIPTION "a high performance cache simulation library")
set(PROJECT_WEB "http://libCacheSim.info")

set(${PROJECT_NAME}_VERSION_MAJOR 0)
set(${PROJECT_NAME}_VERSION_MINOR 1)
set(${PROJECT_NAME}_VERSION_PATCH 0)
set(${PROJECT_NAME}_RELEASE_VERSION ${${PROJECT_NAME}_VERSION_MAJOR}.${${PROJECT_NAME}_VERSION_MINOR})
set(${PROJECT_NAME}_VERSION ${${PROJECT_NAME}_RELEASE_VERSION}.${${PROJECT_NAME}_VERSION_PATCH})


####################
# detect platform #
####################
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    add_definitions(-DOS_DARWIN)

    if (NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Debug)
    endif()
#    set(CMAKE_MACOSX_RPATH 1)
    # Fix linking on 10.14+. See https://stackoverflow.com/questions/54068035
#    LINK_DIRECTORIES(/usr/local/lib)
    # a temporary fix for mac
    #    SET(CMAKE_C_FLAGS "-lglib-2.0")
    #    SET(CMAKE_C_FLAGS "-Wno-unused-command-line-argument -lintl -lglib-2.0 ")
    #    SET(CMAKE_C_FLAGS "-L/usr/local/lib -I/usr/local/Cellar/gettext/0.20.1/include/")
    #    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I/usr/local/Cellar/python/3.7.5/Frameworks/Python.framework/Versions/3.7/include/python3.7m/")
    #    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I/Users/jason/Library/Python/3.7/lib/python/site-packages/numpy/core/include/")

elseif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
#    set(CFLAGS "$ENV{CFLAGS} " "-Wl,--export-dynamic ")
    add_definitions(-DOS_LINUX)
else ()
    message(FATAL_ERROR "unsupported operating system ${CMAKE_SYSTEM_NAME}")
endif ()

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()


####################
# define options #
####################
option(USE_HUGEPAGE "use transparent huge page, can provide % improvement, do `echo madvise | sudo tee /sys/kernel/mm/transparent_hugepage/enabled`" ON)
option(ENABLE_TESTS "whether enable test" ON)
option(ENABLE_LLSC "enable learned log-structured cache" ON)
option(SUPPORT_TTL "whether support TTL" OFF)
#option(LOGLEVEL "log level VVERBOSE 4, VERBOSE 5, DEBUG 6, INFO 7, WARNING 8, SEVERE 9" 6)
set(LOG_LEVEL "debug" CACHE STRING "log level: verbost, debug, info, warning, severe")


if (SUPPORT_TTL)
    add_compile_definitions(SUPPORT_TTL=1)
else()
    add_compile_definitions(SUPPORT_TTL=0)
endif(SUPPORT_TTL)

if (USE_HUGEPAGE)
    add_compile_definitions(USE_HUGEPAGE=1)
else()
    add_compile_definitions(USE_HUGEPAGE=0)
endif(USE_HUGEPAGE)

if (ENABLE_LLSC)
    add_compile_definitions(ENABLE_LLSC=1)
else()
    remove_definitions(ENABLE_LLSC)
endif(ENABLE_LLSC)

if (LOG_LEVEL MATCHES verbose)
    message(STATUS "verbose")
    add_compile_definitions(LOGLEVEL=5)
elseif (LOG_LEVEL MATCHES debug)
    add_compile_definitions(LOGLEVEL=6)
elseif (LOG_LEVEL MATCHES info)
    message(STATUS "info")
    add_compile_definitions(LOGLEVEL=7)
elseif (LOG_LEVEL MATCHES warning)
    message(STATUS "warning")
    add_compile_definitions(LOGLEVEL=8)
elseif (LOG_LEVEL MATCHES severe)
    message(STATUS "severe")
    add_compile_definitions(LOGLEVEL=9)
endif()


message(STATUS "SUPPORT TTL ${SUPPORT_TTL}, USE_HUGEPAGE ${USE_HUGEPAGE}, LOGLEVEL ${LOGLEVEL}, ENABLE_LLSC ${ENABLE_LLSC}")


# set compiler flags
# string concat is easier in 3.0, but older versions don't have the concat subcommand
# so we are using list as input until we move to new version
# add_definitions(-D_GNU_SOURCE -D_FILE_OFFSET_BITS=64)


####################
# flags   #
####################
set(CFLAGS "$ENV{CFLAGS} "
        "-std=gnu99 ${CMAKE_ENABLE_C99} "
        "-Wall -Wshadow -Winline "
        "-Wno-unused "
#                "-Wstrict-prototypes -Wmissing-prototypes "
#                "-Wmissing-declarations "
        "-Wredundant-decls "
#        "-Wl,--export-dynamic "
#                "-Wunused-value -Wunused-variable "
#                "-Wunused-function "
#            -std=c11
#           "-fno-strict-aliasing "
        )


if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(CFLAGS "$ENV{CFLAGS} " "-Wl,--export-dynamic ")
endif ()

string(REPLACE "" "" LOCAL_CFLAGS ${CFLAGS})
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} $ENV{CFLAGS} ${LOCAL_CFLAGS}")


if (CMAKE_BUILD_TYPE MATCHES Debug)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g -ggdb3")     # -DLOGLEVEL=5
else ()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
endif ()


####################
# find dependency #
####################
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

find_package(GLib REQUIRED)
include_directories(${GLib_INCLUDE_DIRS})
set(LIBS ${LIBS} ${GLib_LIBRARY})
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    pkg_check_modules(GLib2 REQUIRED Glib-2.0)
    include_directories(${GLib2_INCLUDE_DIRS})
    #    add_compile_options("${LDFLAGS}")
    link_libraries(${GLib2_LDFLAGS})
endif()

find_package(Tcmalloc)
if ("${Tcmalloc_LIBRARY}" STREQUAL "")
    message(STATUS "!!! cannot find tcmalloc")
else ()
    set(LIBS ${LIBS} ${Tcmalloc_LIBRARIES})
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free")
endif ()

if (ENABLE_LLSC)
find_package(xgboost REQUIRED)
#if (NOT XGBOOST_INCLUDE_DIRS)
#    message(FATAL_ERROR "XGBOOST_INCLUDE_DIR not found")
#endif ()
include_directories(${XGBOOST_INCLUDE_DIR})
link_libraries(xgboost::xgboost)

find_path(LIGHTGBM_INCLUDE_DIR LightGBM/c_api.h REQUIRED)
if (NOT LIGHTGBM_INCLUDE_DIR)
    message(FATAL_ERROR "LIGHTGBM_INCLUDE_DIR not found")
endif ()

find_library(LIGHTGBM_LIB _lightgbm)
if (NOT LIGHTGBM_LIB)
    message(FATAL_ERROR "LIGHTGBM_LIB not found")
endif ()
include_directories(${LIGHTGBM_INCLUDE_DIR})
link_libraries(${LIGHTGBM_LIB})
message(STATUS "${XGBOOST_INCLUDE_DIR} ${LIGHTGBM_LIB} ${LIGHTGBM_INCLUDE_DIR}")
endif()

#find_path(LIGHTGBM_INCLUDE_DIR LightGBM/c_api.h REQUIRED)
#if (NOT LIGHTGBM_INCLUDE_DIR)
#    message(FATAL_ERROR "LIGHTGBM_INCLUDE_DIR not found")
#endif ()
#
#find_library(LIGHTGBM_LIB _lightgbm)
##find_library(LIGHTGBM_LIB lib_lightgbm.so)
#if (NOT LIGHTGBM_LIB)
#    message(FATAL_ERROR "LIGHTGBM_LIB not found")
#endif ()
#target_link_libraries(webcachesim LINK_PUBLIC ${LIGHTGBM_LIB})


link_libraries("-lpthread -ldl -lm ${LIBS}")
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})


message(STATUS "<<++=====------------------\\/------------------=====++>>")
message(STATUS "<<++              libCacheSim summary             ++>>")
message(STATUS "<<++=====------------------/\\------------------=====++>>")

message(STATUS "================== dependency related ==================")
message(STATUS "glib found?     ${GLib_FOUND} - LIBS=${GLib_LIBRARY}, header =${GLib_INCLUDE_DIRS}")
message(STATUS "tcmalloc found? ${Tcmalloc_FOUND} - LIBS=${Tcmalloc_LIBRARIES}, header=${Tcmalloc_INCLUDE_DIRS}")
message(STATUS "lightGBM found? - LIBS=${LIGHTGBM_LIB}, header=${LIGHTGBM_INCLUDE_DIR}")

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
message(STATUS "============= Status of optional features ==============")
message(STATUS "========================================================")


add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libCacheSim)


####################
# library install  #
####################
file(GLOB LIB_SOURCE ${PROJECT_SOURCE_DIR}/libCacheSim/*/*.c)
file(GLOB hashtable ${PROJECT_SOURCE_DIR}/libCacheSim/dataStructure/hashtable/*.c)
file(GLOB hash ${PROJECT_SOURCE_DIR}/libCacheSim/dataStructure/hash/*.c)
set(LIB_SOURCE ${LIB_SOURCE} ${hashtable} ${hash})

set (EXCLUDE_DIR "/bin/")
foreach (TMP_PATH ${LIB_SOURCE})
    string (FIND ${TMP_PATH} ${EXCLUDE_DIR} EXCLUDE_DIR_FOUND)
    if (NOT ${EXCLUDE_DIR_FOUND} EQUAL -1)
        list (REMOVE_ITEM LIB_SOURCE ${TMP_PATH})
    endif ()
endforeach(TMP_PATH)

add_library(${PROJECT_NAME} ${LIB_SOURCE})
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${${PROJECT_NAME}_VERSION})
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE C)

configure_file(${PROJECT_SOURCE_DIR}/${PROJECT_NAME}.pc.in ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc @ONLY)
configure_file(${PROJECT_SOURCE_DIR}/${PROJECT_NAME}.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/Find${PROJECT_NAME}.cmake
        @ONLY)


#install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${CONF_LIB_DIRS})
INSTALL(
        TARGETS ${PROJECT_NAME}
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        COMPONENT library
)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/libCacheSim/include/ DESTINATION include)
install(FILES ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.pc DESTINATION ${CONF_LIB_DIRS}/pkgconfig)
install(FILES ${CMAKE_BINARY_DIR}/Find${PROJECT_NAME}.cmake DESTINATION ${CMAKE_ROOT}/Modules/ COMPONENT dev)




#target_compile_definitions(binBenchmark PRIVATE HASH=0xb2)
#add_executable(binBenchmark libCacheSim/benchmark/main.c)
#target_link_libraries(binBenchmark benchmark cache evictionAlgo traceReader profiler dataStructure traceStat utils
#        ${LIBS})

#add_executable(binDebug ../test/debug.c)
#target_link_libraries(binDebug cache evictionAlgo traceReader profiler dataStructure traceStat utils m ${LIBS} ${CMAKE_THREAD_LIBS_INIT} -lpthread -ldl)
#set_property(TARGET binDebug PROPERTY C_STANDARD 11)


if (ENABLE_TESTS)
    include(CTest)
    enable_testing()
    message(STATUS "Building with test")
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/test)
else ()
    message(STATUS "Building without test")
endif ()



unset(SUPPORT_TTL CACHE)
unset(USE_HUGEPAGE CACHE)
unset(LOGLEVEL CACHE)
unset(ENABLE_LLSC CACHE)


# Example Usages:
# mkdir _build;
# cd _build && rm -r *;
# cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON .. && make -j
# ctest

