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
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Define build options
# echo madvise | sudo tee /sys/kernel/mm/transparent_hugepage/enabled
option(USE_HUGEPAGE "use transparent hugepage" ON)
option(ENABLE_TESTS "whether enable test" ON)
option(ENABLE_GLCACHE "enable group-learned cache" OFF)
option(SUPPORT_TTL "whether support TTL" OFF)
option(OPT_SUPPORT_ZSTD_TRACE "whether support zstd trace" ON)
option(ENABLE_LRB "enable LRB" OFF)
option(ENABLE_3L_CACHE "enable 3LCache" OFF)
option(BUILD_SHARED_LIBS "build shared library" ON)
set(LOG_LEVEL "default" CACHE STRING "change the logging level")
set_property(CACHE LOG_LEVEL PROPERTY STRINGS ERROR WARN INFO DEBUG VERBOSE DEFAULT)

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

configure_file(libCacheSim/include/config.h.in libCacheSim/include/config.h)

if(SUPPORT_TTL)
    add_compile_definitions(SUPPORT_TTL=1)
endif()

if(USE_HUGEPAGE)
    add_compile_definitions(USE_HUGEPAGE=1)
endif()

if(NOT CMAKE_BUILD_TYPE)
    # we can also consider RelWithDebInfo, but it will be too slow for release build
    set(CMAKE_BUILD_TYPE Release)
endif()


string(TOLOWER "${LOG_LEVEL}" LOG_LEVEL_LOWER)
string(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_LOWER)

if(LOG_LEVEL_LOWER STREQUAL "default")
    if(CMAKE_BUILD_TYPE_LOWER MATCHES "debug")
        message(STATUS "LOG_LEVEL is not set, use DEBUG as default for debug build")
        add_compile_definitions(LOGLEVEL=6)
    else()
        message(STATUS "LOG_LEVEL is not set, use INFO as default for release build")
        add_compile_definitions(LOGLEVEL=7)
    endif()
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
list(APPEND dependency_libs ${CMAKE_THREAD_LIBS_INIT})

# Link standard math and dl libraries universally
list(APPEND dependency_libs m dl)

# Find GLib using pkg-config
find_package(PkgConfig REQUIRED)

# Find glib-2.0 using pkg-config
pkg_check_modules(GLib REQUIRED glib-2.0)

# Add GLib library directories to the linker search path
link_directories(${GLib_LIBRARY_DIRS})

# Don't add GLib includes globally - add them to specific targets
# include_directories(${GLib_INCLUDE_DIRS})
list(APPEND dependency_libs ${GLib_LIBRARIES})

find_package(argp REQUIRED)
# Don't add argp includes globally
# include_directories(${ARGP_INCLUDE_DIRS})
list(APPEND dependency_libs ${ARGP_LIBRARY})

if(OPT_SUPPORT_ZSTD_TRACE)
    add_compile_definitions(SUPPORT_ZSTD_TRACE=1)
    find_package(ZSTD)
    message(STATUS "ZSTD_INCLUDE_DIR ${ZSTD_INCLUDE_DIR}, ZSTD_LIBRARIES ${ZSTD_LIBRARIES}")
    if("${ZSTD_LIBRARIES}" STREQUAL "")
        message(FATAL_ERROR "zstd not found")
    else()
        include_directories(${ZSTD_INCLUDE_DIR})
        list(APPEND dependency_libs ${ZSTD_LIBRARIES})
    endif()
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
        list(APPEND dependency_libs ${Tcmalloc_LIBRARIES})
        # tcmalloc flags will be set per target instead of globally
        set(tcmalloc_flags "-fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free")
        list(APPEND LIBCACHESIM_C_FLAGS ${tcmalloc_flags})
    endif()
endif()

find_package(Threads)

if(ENABLE_GLCACHE)
    find_package(xgboost REQUIRED)
    include_directories(${XGBOOST_INCLUDE_DIR})
    list(APPEND dependency_libs xgboost::xgboost)
    add_compile_definitions(ENABLE_GLCACHE=1)
    message(STATUS "XGBOOST_INCLUDE_DIR=${XGBOOST_INCLUDE_DIR}")
endif()

foreach(FEATURE ENABLE_LRB ENABLE_3L_CACHE)
    if(${FEATURE})
        find_path(LIGHTGBM_PATH LightGBM)
        if(NOT LIGHTGBM_PATH)
            message(FATAL_ERROR "LIGHTGBM_PATH not found")
        endif()
        # include_directories(${LIGHTGBM_PATH})

        find_library(LIGHTGBM_LIB _lightgbm)
        if(NOT LIGHTGBM_LIB)
            message(FATAL_ERROR "LIGHTGBM_LIB not found")
        endif()
        list(APPEND dependency_libs ${LIGHTGBM_LIB})
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
set(libCacheSim_include_dir
    ${CMAKE_CURRENT_SOURCE_DIR}/libCacheSim/include
    ${CMAKE_CURRENT_SOURCE_DIR}/libCacheSim/utils/include
    ${CMAKE_CURRENT_SOURCE_DIR}/libCacheSim
)

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
add_library(${PROJECT_NAME} STATIC
    $<TARGET_OBJECTS:cache_lib_c>
    $<TARGET_OBJECTS:cache_lib_cpp>
    $<TARGET_OBJECTS:traceReader_lib>
    $<TARGET_OBJECTS:profiler_lib>
    $<TARGET_OBJECTS:dataStructure_lib>
    $<TARGET_OBJECTS:utils_lib>
    $<TARGET_OBJECTS:traceAnalyzer_lib>
    $<TARGET_OBJECTS:mrcProfiler_lib>
)

# Set include directories for the unified library
target_include_directories(${PROJECT_NAME} INTERFACE
    # Use separate BUILD_INTERFACE for each directory to avoid list expansion issues
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libCacheSim/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libCacheSim/utils/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libCacheSim/>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)


# Link the static library to all required dependencies
target_link_libraries(${PROJECT_NAME} PUBLIC ${dependency_libs})

# Set version and properties
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${${PROJECT_NAME}_VERSION})

# Install the static library
install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}Targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)


if(BUILD_SHARED_LIBS)
    add_library(${PROJECT_NAME}_shared SHARED
        $<TARGET_OBJECTS:cache_lib_c>
        $<TARGET_OBJECTS:cache_lib_cpp>
        $<TARGET_OBJECTS:traceReader_lib>
        $<TARGET_OBJECTS:profiler_lib>
        $<TARGET_OBJECTS:dataStructure_lib>
        $<TARGET_OBJECTS:utils_lib>
        $<TARGET_OBJECTS:traceAnalyzer_lib>
        $<TARGET_OBJECTS:mrcProfiler_lib>
    )

    target_include_directories(${PROJECT_NAME}_shared INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libCacheSim/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libCacheSim/utils/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libCacheSim/>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )

    target_link_libraries(${PROJECT_NAME}_shared PUBLIC ${dependency_libs})

    set_target_properties(${PROJECT_NAME}_shared PROPERTIES VERSION ${${PROJECT_NAME}_VERSION})

    install(TARGETS ${PROJECT_NAME}_shared
        EXPORT ${PROJECT_NAME}Targets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()


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

# Install the export file
install(EXPORT ${PROJECT_NAME}Targets
    FILE ${PROJECT_NAME}Targets.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)
