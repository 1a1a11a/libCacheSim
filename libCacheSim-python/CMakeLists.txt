cmake_minimum_required(VERSION 3.15...3.27)

# Include exported variables from cache
if(DEFINED LIBCB_BUILD_DIR)
    set(PARENT_BUILD_DIR "${LIBCB_BUILD_DIR}")
    message(STATUS "Using provided LIBCB_BUILD_DIR: ${LIBCB_BUILD_DIR}")
else()
    set(PARENT_BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../build")
endif()
set(EXPORT_FILE "${PARENT_BUILD_DIR}/export_vars.cmake")

if(EXISTS "${EXPORT_FILE}")
    include("${EXPORT_FILE}")
    message(STATUS "Loaded variables from export_vars.cmake")
else()
    message(FATAL_ERROR "export_vars.cmake not found at ${EXPORT_FILE}. Please build the main project first (e.g. cd .. && cmake -G Ninja -B build)")
endif()

# Force enable -fPIC
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")

project(libCacheSim-python VERSION "${LIBCACHESIM_VERSION}")

if(LOG_LEVEL_LOWER STREQUAL "default")
    if(CMAKE_BUILD_TYPE_LOWER MATCHES "debug")
        add_compile_definitions(LOGLEVEL=6)
    else()
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
    add_compile_definitions(LOGLEVEL=7)
endif()

# Find python and pybind11
find_package(Python REQUIRED COMPONENTS Interpreter Development.Module)
find_package(pybind11 CONFIG REQUIRED)

# Include directories for dependencies
include_directories(${GLib_INCLUDE_DIRS})
include_directories(${GLib_CONFIG_INCLUDE_DIR})
include_directories(${XGBOOST_INCLUDE_DIR})
include_directories(${LIGHTGBM_PATH})
include_directories(${ZSTD_INCLUDE_DIR})
include_directories(${MAIN_PROJECT_SOURCE_DIR}/libCacheSim/bin)

# Find the main libCacheSim library
set(MAIN_PROJECT_BUILD_DIR "${PARENT_BUILD_DIR}")
set(MAIN_PROJECT_LIB_PATH "${MAIN_PROJECT_BUILD_DIR}/liblibCacheSim.a")

if(EXISTS "${MAIN_PROJECT_LIB_PATH}")
    message(STATUS "Found pre-built libCacheSim library at ${MAIN_PROJECT_LIB_PATH}")

    # Import the main library as an imported target
    add_library(libCacheSim_main STATIC IMPORTED)
    set_target_properties(libCacheSim_main PROPERTIES
        IMPORTED_LOCATION "${MAIN_PROJECT_LIB_PATH}"
        INTERFACE_INCLUDE_DIRECTORIES "${MAIN_PROJECT_SOURCE_DIR}/libCacheSim/include;${MAIN_PROJECT_SOURCE_DIR}/libCacheSim/utils/include;${MAIN_PROJECT_SOURCE_DIR}/libCacheSim"
    )

    # Link dependencies that the main library needs
    target_link_libraries(libCacheSim_main INTERFACE ${dependency_libs})
    set(LIBCACHESIM_TARGET libCacheSim_main)

else()
    message(FATAL_ERROR "Pre-built libCacheSim library not found. Please build the main project first: cd .. && cmake -G Ninja -B build && ninja -C build")
endif()

python_add_library(_libcachesim MODULE
    src/pylibcachesim.cpp
    ${MAIN_PROJECT_SOURCE_DIR}/libCacheSim/bin/cli_reader_utils.c
    WITH_SOABI
)

set_target_properties(_libcachesim PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    INSTALL_RPATH_USE_LINK_PATH TRUE
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "$ORIGIN"
)

target_compile_definitions(_libcachesim PRIVATE VERSION_INFO=${PROJECT_VERSION})

target_link_libraries(_libcachesim PRIVATE
    ${LIBCACHESIM_TARGET}
    pybind11::headers
    pybind11::module
    -Wl,--no-as-needed -ldl
)

# install to wheel directory
install(TARGETS _libcachesim LIBRARY DESTINATION libcachesim)
