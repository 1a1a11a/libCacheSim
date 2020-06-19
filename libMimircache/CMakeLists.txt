cmake_minimum_required(VERSION 2.8)
project(libmimircache C)
set(PROJECT_WEB "http://mimircache.info")

set(${PROJECT_NAME}_VERSION_MAJOR 0)
set(${PROJECT_NAME}_VERSION_MINOR 1)
set(${PROJECT_NAME}_VERSION_REVISION 0)
set(${PROJECT_NAME}_VERSION
    ${${PROJECT_NAME}_VERSION_MAJOR}.${${PROJECT_NAME}_VERSION_MINOR}.${${PROJECT_NAME}_VERSION_REVISION}
    )
set(${PROJECT_NAME}_RELEASE_VERSION
   ${${PROJECT_NAME}_VERSION_MAJOR}.${${PROJECT_NAME}_VERSION_MINOR}
   )


macro(set_platform system_name)
    if(${system_name} MATCHES "Darwin")
        set(OS_PLATFORM "OS_DARWIN")
        add_definitions(-DOS_DARWIN)

    set(CMAKE_BUILD_TYPE Debug)
    set(CMAKE_MACOSX_RPATH 1)
    # Fix linking on 10.14+. See https://stackoverflow.com/questions/54068035
    LINK_DIRECTORIES(/usr/local/lib)
    # a temporary fix for mac
    SET(CMAKE_C_FLAGS_DEBUG " -Wno-unused-command-line-argument -L/usr/local/lib -lglib-2.0 -lintl -I/usr/local/Cellar/gettext/0.20.1/include/")
    SET(CMAKE_C_FLAGS_RELEASE "-Wno-unused-command-line-argument -L/usr/local/lib -lglib-2.0 -lintl -I/usr/local/Cellar/gettext/0.20.1/include/")
    SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -I/usr/local/Cellar/python/3.7.5/Frameworks/Python.framework/Versions/3.7/include/python3.7m/")
    SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -I/Users/jason/Library/Python/3.7/lib/python/site-packages/numpy/core/include/")

    elseif(${system_name} MATCHES "Linux")
        set(OS_PLATFORM "OS_LINUX")
        add_definitions(-DOS_LINUX)
    else()
        set(OS_PLATFORM "OS_UNSUPPORTED")
	message(FATAL_ERROR "unsupported operating system")
    endif()
endmacro(set_platform)

set_platform(${CMAKE_SYSTEM_NAME})



if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()


option(SUPPORT_HUGEPAGE "use transparent huge page, can provide % improvement, do `echo madvise | sudo tee /sys/kernel/mm/transparent_hugepage/enabled`" ON)


# configure_file(
#   "${PROJECT_SOURCE_DIR}/include/config.h.in"
#   "${PROJECT_BINARY_DIR}/include/config.h"
# )

# set compiler flags
# string concat is easier in 3.0, but older versions don't have the concat subcommand
# so we are using list as input until we move to new version
# add_definitions(-D_GNU_SOURCE -D_FILE_OFFSET_BITS=64)

# if(CMAKE_BUILD_TYPE MATCHES Debug)
#     set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0")
# else()
#     set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2")
# endif()

# set(CFLAGS_LIST
#     "-std=c11 "
#     "-ggdb3 "
#     "-Wall -Wshadow -Winline "
#     "-Wstrict-prototypes -Wmissing-prototypes "
#     "-Wmissing-declarations -Wredundant-decls "
#     "-Wunused-function -Wunused-value -Wunused-variable "
#     "-fno-strict-aliasing ")



SET(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG}   $ENV{CFLAGS} -O0 -Wall -g -ggdb3 -Wno-unused -std=gnu99 ${CMAKE_ENABLE_C99} -DMIMIR_LOGLEVEL=6")
SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} $ENV{CFLAGS} -O3 -Wall -Wno-unused -std=gnu99 ${CMAKE_ENABLE_C99} -DMIMIR_LOGLEVEL=7")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

find_package(GLib REQUIRED)
include_directories(${GLib_INCLUDE_DIRS})
set(LIBS ${LIBS} ${GLib_LIBRARY})
message(STATUS "glib found? " ${GLib_FOUND} " LIBS = " ${LIBS} ", header = " ${GLib_INCLUDE_DIRS})
# if(NOT GLib_FOUND)
#     message(FATAL_ERROR "-- #### ERROR ####: Glib library not found")
# endif()


find_package(Tcmalloc REQUIRED)
message(STATUS "Tcmalloc found? " ${Tcmalloc_FOUND} " LIBS = " ${Tcmalloc_LIBRARIES} ", header = " ${Tcmalloc_INCLUDE_DIRS})
# if(NOT Tcmalloc_FOUND)
#     message("-- #### WARNING ####: tcmalloc not found, use default malloc")
# else()
#    include_directories(${Tcmalloc_INCLUDE_DIR})
    set(LIBS ${LIBS} ${Tcmalloc_LIBRARIES})
    set(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG}   -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free")
# endif()

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


message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "CMAKE_CURRENT_SOURCE_DIR = " ${CMAKE_CURRENT_SOURCE_DIR})
message(STATUS "CMAKE_C_COMPILER = " ${CMAKE_C_COMPILER})

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/headers)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/dataStructure/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/cache/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/cacheAlgo/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/traceReader/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/readerUtils/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/profiler/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/utils/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/benchmark/include)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dataStructure)
#add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/cache)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/cacheAlgo)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/traceReader)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/readerUtils)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/profiler)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/utils)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/benchmark)

add_library(${PROJECT_NAME} SHARED ${ALL_SRC_FILES} ${LIBS} ${CMAKE_THREAD_LIBS_INIT})
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${libmimircache_VERSION_MAJOR}.${libmimircache_VERSION_MINOR}.${libmimircache_VERSION_REVISION})
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE C)
target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include)

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION lib)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/ DESTINATION include)


file(GLOB ALL_SRC_FILES ${PROJECT_SOURCE_DIR}/*/*.c)
# file(GLOB py_SRC_FILES ${PROJECT_SOURCE_DIR}/../../pyBindings/*.c)
# add_library(Py${PROJECT_NAME} SHARED ${ALL_SRC_FILES} ${py_SRC_FILES} ${LIBS} ${CMAKE_THREAD_LIBS_INIT})
# set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${mimircache_VERSION_MAJOR}.${mimircache_VERSION_MINOR}.${mimircache_VERSION_REVISION})
# set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE C)
# target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include)


message(STATUS "<<++=====------------------\\/------------------=====++>>")
message(STATUS "<<++             libmimircache summary               ++>>")
message(STATUS "<<++=====------------------/\\------------------=====++>>")
message(STATUS "=============CMake related=============")
message(STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE})
message(STATUS "PLATFORM: " ${OS_PLATFORM})
message(STATUS "CPPFLAGS: " ${CMAKE_CPP_FLAGS})
message(STATUS "CFLAGS: " ${CMAKE_C_FLAGS})
message(STATUS "=======================================")

# message(STATUS "=======Status of system features=======")
# message(STATUS "HAVE_BACKTRACE: " ${HAVE_BACKTRACE})
# message(STATUS "=======================================")

message(STATUS "======Status of optional features======")
message(STATUS "HAVE_RUST: " ${HAVE_RUST})
# message(STATUS "HAVE_ITT_INSTRUMENTATION: " ${HAVE_ITT_INSTRUMENTATION})
# message(STATUS "HAVE_TEST: " ${HAVE_TEST})
message(STATUS "=======================================")

























#target_compile_definitions(binBenchmark PRIVATE HASH=0xb2)
add_executable(binBenchmark benchmark/main.c)
target_link_libraries(binBenchmark benchmark cacheAlgo traceReader profiler dataStructure readerUtils utils m ${LIBS} ${CMAKE_THREAD_LIBS_INIT} -lpthread -ldl)

add_executable(testReader ../test/test_traceReader.c)
target_link_libraries(testReader cacheAlgo traceReader profiler dataStructure readerUtils utils m ${LIBS} ${CMAKE_THREAD_LIBS_INIT} -lpthread -ldl)

add_executable(testDistUtils ../test/test_distUtils.c)
target_link_libraries(testDistUtils cacheAlgo traceReader profiler dataStructure readerUtils utils m ${LIBS} ${CMAKE_THREAD_LIBS_INIT} -lpthread -ldl)

add_executable(testProfilerLRU ../test/test_profilerLRU.c)
target_link_libraries(testProfilerLRU cacheAlgo traceReader profiler dataStructure readerUtils utils m ${LIBS} ${CMAKE_THREAD_LIBS_INIT} -lpthread -ldl)

add_executable(testProfiler ../test/test_profiler.c)
target_link_libraries(testProfiler cacheAlgo traceReader profiler dataStructure readerUtils utils m ${LIBS} ${CMAKE_THREAD_LIBS_INIT} -lpthread -ldl)

add_executable(testCacheAlgo ../test/test_cacheAlgo.c)
target_link_libraries(testCacheAlgo cacheAlgo traceReader profiler dataStructure readerUtils utils m ${LIBS} ${CMAKE_THREAD_LIBS_INIT} -lpthread -ldl)

add_executable(testHeatmap ../test/test_heatmap.c)
target_link_libraries(testHeatmap cacheAlgo traceReader profiler dataStructure readerUtils utils m ${LIBS} ${CMAKE_THREAD_LIBS_INIT} -lpthread -ldl)

#add_executable(binDebug ../test/debug.c)
#target_link_libraries(binDebug cacheAlgo traceReader profiler dataStructure readerUtils utils m ${LIBS} ${CMAKE_THREAD_LIBS_INIT} -lpthread -ldl)
#set_property(TARGET binDebug PROPERTY C_STANDARD 11)

enable_testing()
add_test(NAME testReader COMMAND testReader WORKING_DIRECTORY .)
add_test(NAME testDistUtils COMMAND testDistUtils WORKING_DIRECTORY .)
add_test(NAME testProfilerLRU COMMAND testProfilerLRU WORKING_DIRECTORY .)
add_test(NAME testProfiler COMMAND testProfiler WORKING_DIRECTORY .)
add_test(NAME testCacheAlgo COMMAND testCacheAlgo WORKING_DIRECTORY .)



# performance related
# change optimization to O3 (O2 is 100% faster than O0, O3 is 5% faster than O2)


# enable transparent hugepage (30% improvement 5.05s to 3.65s)
# echo always | sudo tee /sys/kernel/mm/transparent_hugepage/enabled
#echo always | sudo tee /sys/kernel/mm/transparent_hugepage/defrag

# add pg profiler support in compilation
# cmake -DCMAKE_CXX_FLAGS=-pg -DCMAKE_EXE_LINKER_FLAGS=-pg -DCMAKE_SHARED_LINKER_FLAGS=-pg ..

# add heap profiler
    # LD_PRELOAD=/home/jason/software/source/gperftools-2.7/.libs/libtcmalloc.so HEAPPROFILE=heap ./binBenchmark LRU
    # pprof --pdf ./binBenchmark heap.0001.heap > a.pdf

# profiling
    # perf event counter intel   https://download.01.org/perfmon/index/
    #
    # perf stat -e dTLB-loads,dTLB-load-misses,dTLB-stores,dTLB-store-misses -a -I 1000
    #   https://alexandrnikitin.github.io/blog/transparent-hugepages-measuring-the-performance-impact/
    #
    # perf stat -e task-clock,cycles,instructions,cache-references,cache-misses

# performance results
# memory align 60s -> 10.8s
# change to tcmalloc 10.8s -> 10.6s
# hashtable re-impl 10s->6s 1966 MB->1400MB
# huge page 6s->3.6s
# various other such as likely() and unlikely()



#########
# two mistakes I have made in benchmarking the lib,
# 1. this is actually not a mistake in benchmark, rather than a mistake in design, I used pointer to store integer obj_id,
# which has severe performance issue when the obj_id % MEM_ALIGN != 0, although the memory location is not used.
# 2. to correct this error, I start obj_id with MEM_ALIGN and add MEM_ALIGN in each step, then the performance is amazing,
# then when I compare this with trace replay, the trace replay is much worse, after investigation, I realize, since I add
# MEM_ALIGN each time, CPU is able to prefetch, which boosts the performance by more than 2-4x.






# Example Usages
# mkdir _build; cd build
# rm -r *; cmake .. -G Ninja; ninja
# rm -r *; cmake ..; make -j


# cmake -DCMAKE_BUILD_TYPE=Debug ..
# cmake -DCMAKE_BUILD_TYPE=Release ..
# cmake -DBUILD_TESTS=ON ..; make -j; ctest


