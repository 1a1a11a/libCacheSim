cmake_minimum_required(VERSION 2.8)

project(mimircache C)
set(PROJECT_WEB "http://mimircache.info")
set(mimircache_VERSION_MAJOR 0)
set(mimircache_VERSION_MINOR 1)
set(mimircache_VERSION_REVISION 0)
set(CMAKE_BUILD_TYPE Debug)

# clear an error when using cmake in macOS
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(CMAKE_MACOSX_RPATH 1)
    # Fix linking on 10.14+. See https://stackoverflow.com/questions/54068035
    LINK_DIRECTORIES(/usr/local/lib)
    # a temporary fix for mac
    SET(CMAKE_C_FLAGS_DEBUG " -L/usr/local/lib -lglib-2.0 -lintl -I/usr/local/Cellar/gettext/0.20.1/include/")
    SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wno-unused-command-line-argument")
    SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -I/usr/local/Cellar/python/3.7.5/Frameworks/Python.framework/Versions/3.7/include/python3.7m/")
    SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -I/Users/jason/Library/Python/3.7/lib/python/site-packages/numpy/core/include/")

    SET(CMAKE_C_FLAGS_RELEASE " -L/usr/local/lib -lglib-2.0 -lintl -Wno-unused-command-line-argument -I/usr/local/Cellar/gettext/0.20.1/include/")
endif ()


# add_compile_options(-std=c++11)
SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} $ENV{CFLAGS} -O0 -Wall -g -ggdb -Wno-unused -DLOGLEVEL=6")
SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} $ENV{CFLAGS} -O3 -Wall -ggdb -Wno-unused -DLOGLEVEL=5")
#SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb -Wno-unused -DLOGGING_DEBUG")
#SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall -DLOGGING_INFO")
message(STATUS "FLAG" ${CMAKE_C_FLAGS_DEBUG})


set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
# find libglib2.0-dev
find_package(GLib "2.40" REQUIRED)
include_directories(${GLib_INCLUDE_DIRS})
set(LIBS ${LIBS} ${GLib_LIBRARY})
message(STATUS "glib found? " ${GLib_FOUND} ", LIBS = " ${LIBS} ", header = " ${GLib_INCLUDE_DIRS})


#aux_source_directory(../example dirmimircache)
#message(STATUS "mimircache sources = " ${dirmimircache})


include_directories(${CMAKE_CURRENT_SOURCE_DIR}/headers)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/dataStructure/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/cache/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/cacheReader/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/profiler/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/utils/include)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/cache)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/cacheReader)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dataStructure)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/profiler)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/utils)

file(GLOB ALL_SRC_FILES ${PROJECT_SOURCE_DIR}/*/*.c)
#message(STATUS "all sources " ${ALL_SRC_FILES})

#add_library(${PROJECT_NAME} SHARED cache cacheReader run_trace dataStructure utils ${LIBS} ${CMAKE_THREAD_LIBS_INIT})
add_library(${PROJECT_NAME} SHARED ${ALL_SRC_FILES} ${LIBS} ${CMAKE_THREAD_LIBS_INIT})
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${mimircache_VERSION_MAJOR}.${mimircache_VERSION_MINOR}.${mimircache_VERSION_REVISION})
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE C)
target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include)

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION lib)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/ DESTINATION include)


# file(GLOB py_SRC_FILES ${PROJECT_SOURCE_DIR}/../../pyBindings/*.c)
# add_library(Py${PROJECT_NAME} SHARED ${ALL_SRC_FILES} ${py_SRC_FILES} ${LIBS} ${CMAKE_THREAD_LIBS_INIT})
# set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${mimircache_VERSION_MAJOR}.${mimircache_VERSION_MINOR}.${mimircache_VERSION_REVISION})
# set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE C)
# target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include)


add_executable(readerTest ../test/test_reader.c)
target_link_libraries(readerTest cache cacheReader profiler dataStructure utils m ${LIBS} ${CMAKE_THREAD_LIBS_INIT} -lpthread -ldl)

add_executable(distUtilsTest ../test/test_distUtils.c)
target_link_libraries(distUtilsTest cache cacheReader profiler dataStructure utils m ${LIBS} ${CMAKE_THREAD_LIBS_INIT} -lpthread -ldl)

add_executable(lruProfilerTest ../test/test_lruProfiler.c)
target_link_libraries(lruProfilerTest cache cacheReader profiler dataStructure utils m ${LIBS} ${CMAKE_THREAD_LIBS_INIT} -lpthread -ldl)

add_executable(profilerTest ../test/test_profiler.c)
target_link_libraries(profilerTest cache cacheReader profiler dataStructure utils m ${LIBS} ${CMAKE_THREAD_LIBS_INIT} -lpthread -ldl)

enable_testing()
add_test(NAME readerTest COMMAND readerTest WORKING_DIRECTORY .)
add_test(NAME distUtils COMMAND lruProfilerTest WORKING_DIRECTORY .)
add_test(NAME lruProfilerTest COMMAND lruProfilerTest WORKING_DIRECTORY .)
add_test(NAME profilerTest COMMAND lruProfilerTest WORKING_DIRECTORY .)


# Example Usages
# mkdir _build; cd build
# rm -r *; cmake .. -G Ninja; ninja
# rm -r *; cmake ..; make -j


# cmake -DCMAKE_BUILD_TYPE=Debug ..
# cmake -DCMAKE_BUILD_TYPE=Release ..
# cmake -DBUILD_TESTS=ON ..; make -j; ctest
