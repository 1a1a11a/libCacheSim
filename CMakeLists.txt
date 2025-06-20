cmake_minimum_required(VERSION 3.12)
project(libCacheSim)
set(DESCRIPTION "a high performance cache simulation library")
set(PROJECT_WEB "http://cachesim.com")

# Include version utilities
include(cmake/Version.cmake)

# Setup version from version.txt
setup_project_version(${PROJECT_NAME} "${CMAKE_CURRENT_SOURCE_DIR}/version.txt")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED On)
set(CMAKE_CXX_EXTENSIONS Off)

# export compile commands, useful for IDEs
set(EXPORT_COMPILE_COMMANDS ON)

# Define build options
# echo madvise | sudo tee /sys/kernel/mm/transparent_hugepage/enabled
option(USE_HUGEPAGE "use transparent hugepage" ON)
option(ENABLE_TESTS "whether enable test" ON)
option(ENABLE_GLCACHE "enable group-learned cache" OFF)
option(SUPPORT_TTL "whether support TTL" OFF)
option(OPT_SUPPORT_ZSTD_TRACE "whether support zstd trace" ON)
option(ENABLE_LRB "enable LRB" OFF)
option(ENABLE_3L_CACHE "enable 3LCache" OFF)
set(LOG_LEVEL NONE CACHE STRING "change the logging level")
set_property(CACHE LOG_LEVEL PROPERTY STRINGS INFO WARN ERROR DEBUG VERBOSE VVERBOSE VVVERBOSE)

# Platform detection
if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    message(STATUS "Mac OS X detected, version ${CMAKE_SYSTEM_VERSION}")
    add_definitions(-DOS_DARWIN)
    set(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH} "/opt/homebrew/include/")
    set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "/opt/homebrew/")

elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    add_definitions(-DOS_LINUX)
else()
    message(FATAL_ERROR "unsupported operating system ${CMAKE_SYSTEM_NAME}")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)

    if(LOG_LEVEL STREQUAL "NONE")
        set(LOG_LEVEL INFO)
    endif()
elseif(${CMAKE_BUILD_TYPE} MATCHES "Debug")
    if(LOG_LEVEL STREQUAL "NONE")
        set(LOG_LEVEL DEBUG)
    endif()
else()
    set(LOG_LEVEL INFO)
endif()

configure_file(libCacheSim/include/config.h.in libCacheSim/include/config.h)

if(SUPPORT_TTL)
    add_compile_definitions(SUPPORT_TTL=1)
endif()

if(USE_HUGEPAGE)
    add_compile_definitions(USE_HUGEPAGE=1)
endif()

string(TOLOWER "${LOG_LEVEL}" LOG_LEVEL_LOWER)

if(LOG_LEVEL_LOWER STREQUAL "vvverbose")
    add_compile_definitions(LOGLEVEL=3)
elseif(LOG_LEVEL_LOWER STREQUAL "vverbose")
    add_compile_definitions(LOGLEVEL=4)
elseif(LOG_LEVEL_LOWER STREQUAL "verbose")
    add_compile_definitions(LOGLEVEL=5)
elseif(LOG_LEVEL_LOWER STREQUAL "debug")
    add_compile_definitions(LOGLEVEL=6)
elseif(LOG_LEVEL_LOWER STREQUAL "info")
    add_compile_definitions(LOGLEVEL=7)
elseif(LOG_LEVEL_LOWER STREQUAL "warn")
    add_compile_definitions(LOGLEVEL=8)
elseif(LOG_LEVEL_LOWER STREQUAL "error")
    add_compile_definitions(LOGLEVEL=9)

# default none is info
elseif(LOG_LEVEL_LOWER STREQUAL "none")
    add_compile_definitions(LOGLEVEL=7)
else()
    message(WARNING "unknown log level ${LOG_LEVEL}, use INFO as default")
    add_compile_definitions(LOGLEVEL=7)
endif()

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -Wno-unused-variable -Wno-unused-function -Wno-unused-parameter -Wno-unused-but-set-variable -Wpedantic -Wformat=2 -Wformat-security -Wshadow -Wwrite-strings -Wstrict-prototypes -Wold-style-definition -Wredundant-decls -Wnested-externs -Wmissing-include-dirs")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Wno-deprecated-copy -Wno-unused-variable -Wno-unused-function -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-pedantic -Wformat=2 -Wformat-security -Wshadow -Wwrite-strings -Wmissing-include-dirs")

# Find dependencies
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

# Include GNUInstallDirs for standard installation directories
include(GNUInstallDirs)

find_package(GLib REQUIRED)
include_directories(${GLib_INCLUDE_DIRS})
set(LIBS ${LIBS} ${GLib_LIBRARY})

find_package(argp REQUIRED)
include_directories(${ARGP_INCLUDE_DIRS})
set(LIBS ${LIBS} ${ARGP_LIBRARY})

if(OPT_SUPPORT_ZSTD_TRACE)
    add_compile_definitions(SUPPORT_ZSTD_TRACE=1)
    find_package(ZSTD)

    include_directories(${ZSTD_INCLUDE_DIR})

    if("${ZSTD_LIBRARIES}" STREQUAL "")
        message(FATAL_ERROR "zstd not found")
    endif()

    set(LIBS ${LIBS} ${ZSTD_LIBRARIES})
    message(STATUS "ZSTD_INCLUDE_DIR ${ZSTD_INCLUDE_DIR}, ZSTD_LIBRARIES ${ZSTD_LIBRARIES}")
endif()

# TCMalloc (libgoogle-perftools-dev google-perftools)
# Note: tcmalloc causes trouble with valgrind
# https://github.com/gperftools/gperftools/issues/792
# When using valgrind, we should not compile with tcmalloc
if(NOT ${CMAKE_BUILD_TYPE} MATCHES "Debug")
    find_package(Tcmalloc)

    if("${Tcmalloc_LIBRARY}" STREQUAL "")
        message(STATUS "!!! cannot find tcmalloc")
    else()
        set(LIBS ${LIBS} ${Tcmalloc_LIBRARIES})
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free")
    endif()
endif()

find_package(Threads)

if(ENABLE_GLCACHE)
    find_package(xgboost REQUIRED)
    include_directories(${XGBOOST_INCLUDE_DIR})
    set(LIBS ${LIBS} xgboost::xgboost)
    add_compile_definitions(ENABLE_GLCACHE=1)
    message(STATUS "XGBOOST_INCLUDE_DIR=${XGBOOST_INCLUDE_DIR}")
endif()

foreach(FEATURE ENABLE_LRB ENABLE_3L_CACHE)
    if(${FEATURE})
        find_path(LIGHTGBM_PATH LightGBM)
        if(NOT LIGHTGBM_PATH)
            message(FATAL_ERROR "LIGHTGBM_PATH not found")
        endif()
        include_directories(${LIGHTGBM_PATH})

        find_library(LIGHTGBM_LIB _lightgbm)
        if(NOT LIGHTGBM_LIB)
            message(FATAL_ERROR "LIGHTGBM_LIB not found")
        endif()
        set(LIBS ${LIBS} ${LIGHTGBM_LIB})
        add_compile_definitions(${FEATURE}=1)
    endif()
endforeach()
# Put binary in bin directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Status messages
message(STATUS "CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "CMAKE_CXX_FLAGS_RELWITHDEBINFO ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
message(STATUS "CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE}")
message(STATUS "SUPPORT TTL ${SUPPORT_TTL}, USE_HUGEPAGE ${USE_HUGEPAGE}")
message(STATUS "LOGLEVEL ${LOG_LEVEL}, ENABLE_GLCACHE ${ENABLE_GLCACHE}")
message(STATUS "ENABLE_LRB ${ENABLE_LRB}, ENABLE_3L_CACHE ${ENABLE_3L_CACHE}")
message(STATUS "OPT_SUPPORT_ZSTD_TRACE ${OPT_SUPPORT_ZSTD_TRACE}")

message(STATUS "<<++=====------------------\\/------------------=====++>>")
message(STATUS "<<++              libCacheSim summary             ++>>")
message(STATUS "<<++=====------------------/\\------------------=====++>>")

message(STATUS "================== dependency related ==================")
message(STATUS "glib found?     ${GLib_FOUND} - LIBS=${GLib_LIBRARY}, header=${GLib_INCLUDE_DIRS}")
message(STATUS "tcmalloc found? ${Tcmalloc_FOUND} - LIBS=${Tcmalloc_LIBRARIES}, header=${Tcmalloc_INCLUDE_DIRS}")
message(STATUS "ZSTD found? ${ZSTD_FOUND} - LIBS=${ZSTD_LIBRARIES}, header=${ZSTD_INCLUDE_DIR}")

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

# Library compilation
# Define include directories for reuse in subdirectories
set(libCacheSim_include_dir ${CMAKE_CURRENT_SOURCE_DIR}/libCacheSim/include)
set(libCacheSim_binary_include_dir ${CMAKE_CURRENT_BINARY_DIR}/libCacheSim/include)

include_directories(${libCacheSim_include_dir})
include_directories(${libCacheSim_binary_include_dir})
set(ALL_MODULES cachelib admissionC admissionCpp prefetchC evictionC evictionCPP traceReader profiler dataStructure ds_hash utils)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libCacheSim/cache)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libCacheSim/dataStructure)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libCacheSim/traceReader)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libCacheSim/profiler)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libCacheSim/utils)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libCacheSim/bin)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libCacheSim/traceAnalyzer)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libCacheSim/mrcProfiler)

# Library installation
# Compile a single library
file(GLOB LIB_SOURCE ${PROJECT_SOURCE_DIR}/libCacheSim/*.c)

file(GLOB cache_source
    ${PROJECT_SOURCE_DIR}/libCacheSim/cache/*.c
    ${PROJECT_SOURCE_DIR}/libCacheSim/cache/eviction/*.c
    ${PROJECT_SOURCE_DIR}/libCacheSim/cache/admission/*.c
    ${PROJECT_SOURCE_DIR}/libCacheSim/cache/prefetch/*.c

    ${PROJECT_SOURCE_DIR}/libCacheSim/cache/eviction/LHD/*
    ${PROJECT_SOURCE_DIR}/libCacheSim/cache/eviction/cpp/*
)

if(EXISTS ${PROJECT_SOURCE_DIR}/libCacheSim/cache/eviction/fifo)
    file(GLOB fifo_source
        ${PROJECT_SOURCE_DIR}/libCacheSim/cache/eviction/fifo/*.c)
    set(cache_source ${cache_source} ${fifo_source})
endif()

if(EXISTS ${PROJECT_SOURCE_DIR}/libCacheSim/cache/eviction/belady)
    file(GLOB belady_source
        ${PROJECT_SOURCE_DIR}/libCacheSim/cache/eviction/belady/*.c)
    set(cache_source ${cache_source} ${belady_source})
endif()

if(EXISTS ${PROJECT_SOURCE_DIR}/libCacheSim/cache/eviction/priv)
    file(GLOB priv_source
        ${PROJECT_SOURCE_DIR}/libCacheSim/cache/eviction/priv/*.c)
    set(cache_source ${cache_source} ${priv_source})
endif()

if(ENABLE_GLCACHE)
    file(GLOB GLCache_source
        ${PROJECT_SOURCE_DIR}/libCacheSim/cache/eviction/GLCache/*.c)
    set(cache_source ${cache_source} ${GLCache_source})
endif()

if(ENABLE_LRB)
    file(GLOB LRB_source
        ${PROJECT_SOURCE_DIR}/libCacheSim/cache/eviction/LRB/*.cpp)
    set(cache_source ${cache_source} ${LRB_source})
endif()

if(ENABLE_3L_CACHE)
    file(GLOB ThreeLCache_source
        ${PROJECT_SOURCE_DIR}/libCacheSim/cache/eviction/3LCache/*.cpp)
    set(cache_source ${cache_source} ${ThreeLCache_source})
endif()

set(reader_source
    ${PROJECT_SOURCE_DIR}/libCacheSim/traceReader/reader.c
    ${PROJECT_SOURCE_DIR}/libCacheSim/traceReader/generalReader/binary.c
    ${PROJECT_SOURCE_DIR}/libCacheSim/traceReader/generalReader/csv.c
    ${PROJECT_SOURCE_DIR}/libCacheSim/traceReader/customizedReader/lcs.c
    ${PROJECT_SOURCE_DIR}/libCacheSim/traceReader/generalReader/libcsv.c
    ${PROJECT_SOURCE_DIR}/libCacheSim/traceReader/generalReader/txt.c
)

if(OPT_SUPPORT_ZSTD_TRACE)
    set(reader_source ${reader_source}
        ${PROJECT_SOURCE_DIR}/libCacheSim/traceReader/generalReader/zstdReader.c)
endif()

file(GLOB dataStructure_source
    ${PROJECT_SOURCE_DIR}/libCacheSim/dataStructure/*.c
    ${PROJECT_SOURCE_DIR}/libCacheSim/dataStructure/hashtable/*.c
    ${PROJECT_SOURCE_DIR}/libCacheSim/dataStructure/hash/murmur3.c
)

file(GLOB profiler_source
    ${PROJECT_SOURCE_DIR}/libCacheSim/profiler/*.c
)

file(GLOB utils_source
    ${PROJECT_SOURCE_DIR}/libCacheSim/utils/*.c
)

set(LIB_SOURCE ${LIB_SOURCE} ${cache_source} ${reader_source}
    ${dataStructure_source} ${profiler_source} ${utils_source})

# Build configuration
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

add_library(${PROJECT_NAME} ${LIB_SOURCE})
target_link_options(${PROJECT_NAME} PRIVATE -Wl,--export-dynamic)

# Link against found dependencies
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBS})
if(CMAKE_THREAD_LIBS_INIT)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_THREAD_LIBS_INIT})
endif()
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${${PROJECT_NAME}_VERSION})
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE C)
set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER ${PROJECT_SOURCE_DIR}/libCacheSim/include/libCacheSim.h)

configure_file(${PROJECT_SOURCE_DIR}/${PROJECT_NAME}.pc.in ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc @ONLY)
configure_file(${PROJECT_SOURCE_DIR}/${PROJECT_NAME}.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/Find${PROJECT_NAME}.cmake @ONLY)

# Install configuration
install(DIRECTORY ${PROJECT_SOURCE_DIR}/libCacheSim/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/Find${PROJECT_NAME}.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME} COMPONENT dev)

install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Testing configuration
if(ENABLE_TESTS)
    include(CTest)
    enable_testing()
    message(STATUS "Building with test")
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/test)
else()
    message(STATUS "Building without test")
endif()
