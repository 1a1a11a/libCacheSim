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

# Define shared compiler flags for all targets
set(LIBCACHESIM_C_FLAGS 
    -Wall -Wextra -Werror 
    -Wno-unused-variable -Wno-unused-function -Wno-unused-parameter -Wno-unused-but-set-variable 
    -Wpedantic -Wformat=2 -Wformat-security -Wshadow -Wwrite-strings 
    -Wstrict-prototypes -Wold-style-definition -Wredundant-decls -Wnested-externs -Wmissing-include-dirs
)

set(LIBCACHESIM_CXX_FLAGS 
    -Wall -Wextra -Werror 
    -Wno-deprecated-copy -Wno-unused-variable -Wno-unused-function -Wno-unused-parameter -Wno-unused-but-set-variable 
    -Wno-pedantic -Wformat=2 -Wformat-security -Wshadow -Wwrite-strings -Wmissing-include-dirs
)

# make sure the symbols are exported so that we can use dlsym
set(CMAKE_C_VISIBILITY_PRESET default)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 0)

# Find dependencies
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

# Include GNUInstallDirs for standard installation directories
include(GNUInstallDirs)

set(dependency_libs "")

find_package(Threads REQUIRED)
set(dependency_libs ${dependency_libs} ${CMAKE_THREAD_LIBS_INIT})

find_package(GLib REQUIRED)
include_directories(${GLib_INCLUDE_DIRS})
set(dependency_libs ${dependency_libs} ${GLib_LIBRARY})

find_package(argp REQUIRED)
include_directories(${ARGP_INCLUDE_DIRS})
set(dependency_libs ${dependency_libs} ${ARGP_LIBRARY})

if(OPT_SUPPORT_ZSTD_TRACE)
    add_compile_definitions(SUPPORT_ZSTD_TRACE=1)
    find_package(ZSTD)

    include_directories(${ZSTD_INCLUDE_DIR})

    if("${ZSTD_LIBRARIES}" STREQUAL "")
        message(FATAL_ERROR "zstd not found")
    endif()

    set(dependency_libs ${dependency_libs} ${ZSTD_LIBRARIES})
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
        set(dependency_libs ${dependency_libs} ${Tcmalloc_LIBRARIES})
        # tcmalloc flags will be set per target instead of globally
        set(tcmalloc_flags "-fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free")
    endif()
endif()

find_package(Threads)

if(ENABLE_GLCACHE)
    find_package(xgboost REQUIRED)
    include_directories(${XGBOOST_INCLUDE_DIR})
    set(dependency_libs ${dependency_libs} xgboost::xgboost)
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
        set(dependency_libs ${dependency_libs} ${LIGHTGBM_LIB})
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
message(STATUS "dependency_libs    = ${dependency_libs}")
message(STATUS "Installation path = ${CMAKE_INSTALL_PREFIX}")

message(STATUS "========================================================")
message(STATUS "============= Status of optional features ==============")
message(STATUS "========================================================")

# Define include directories for reuse in subdirectories
set(libCacheSim_include_dir ${CMAKE_CURRENT_SOURCE_DIR}/libCacheSim/include ${CMAKE_CURRENT_SOURCE_DIR}/libCacheSim/utils/include ${CMAKE_CURRENT_SOURCE_DIR}/libCacheSim)
set(libCacheSim_binary_include_dir ${CMAKE_CURRENT_BINARY_DIR}/libCacheSim/include)

include_directories(${libCacheSim_include_dir})
include_directories(${libCacheSim_binary_include_dir})
# Restructured to avoid transitive dependency duplicates
# profiler_lib already includes cache_lib_c, cache_lib_cpp, traceReader_lib (which includes utils_lib)
# Only include top-level libraries to avoid duplicates
set(all_modules cache_lib_c cache_lib_cpp traceReader_lib profiler_lib dataStructure_lib utils_lib traceAnalyzer_lib mrcProfiler_lib)
# For linking: use minimal sets to avoid transitive dependency duplicates
set(core_libs cache_lib_c cache_lib_cpp traceReader_lib dataStructure_lib utils_lib)
set(high_level_libs profiler_lib traceAnalyzer_lib)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libCacheSim/cache)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libCacheSim/dataStructure)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libCacheSim/traceReader)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libCacheSim/profiler)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libCacheSim/utils)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libCacheSim/traceAnalyzer)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libCacheSim/mrcProfiler)

# libCacheSim binary / tool compilation
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libCacheSim/bin)

# libCacheSim test
if(ENABLE_TESTS)
    include(CTest)
    enable_testing()
    message(STATUS "Building with test")
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/test)
else()
    message(STATUS "Building without test")
endif()


# libCacheSim unified library compilation and installation
# Create a single library that combines all modular libraries
add_library(${PROJECT_NAME} INTERFACE)

# Link all modular libraries into the unified library
target_link_libraries(${PROJECT_NAME} INTERFACE ${high_level_libs})

# Set include directories for the unified library
target_include_directories(${PROJECT_NAME} INTERFACE
    $<BUILD_INTERFACE:${libCacheSim_include_dir}>
    $<BUILD_INTERFACE:${libCacheSim_binary_include_dir}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Link dependencies for the unified library
target_link_libraries(${PROJECT_NAME} INTERFACE ${dependency_libs})

# Set version and properties
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${${PROJECT_NAME}_VERSION})

# Configuration files
configure_file(${PROJECT_SOURCE_DIR}/${PROJECT_NAME}.pc.in ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc @ONLY)
configure_file(${PROJECT_SOURCE_DIR}/${PROJECT_NAME}.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/Find${PROJECT_NAME}.cmake @ONLY)

# Install configuration
install(DIRECTORY ${PROJECT_SOURCE_DIR}/libCacheSim/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/Find${PROJECT_NAME}.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME} COMPONENT dev)

# Install all modular libraries and the unified interface library
install(TARGETS ${PROJECT_NAME} ${all_modules}
    EXPORT ${PROJECT_NAME}Targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install the export file
install(EXPORT ${PROJECT_NAME}Targets
    FILE ${PROJECT_NAME}Targets.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

