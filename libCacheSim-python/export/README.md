# libCacheSim Python Binding Export

This directory contains the export mechanism for sharing variables between the main libCacheSim project and the Python binding.

## Overview

The `export/CMakeLists.txt` file serves as a bridge between the main libCacheSim project and the Python binding, ensuring that all necessary variables (source files, include directories, compiler flags, etc.) are properly exported and can be imported by the Python binding's CMakeLists.txt.

## How It Works

### 1. Variable Export Process

The export mechanism works in the following steps:

1. **Path Conversion**: Converts relative source file paths to absolute paths using the `convert_to_absolute_paths` function
2. **Variable Collection**: Gathers all necessary variables from the main project
3. **File Generation**: Writes all variables to `export_vars.cmake` in the build directory
4. **Import**: The Python binding's CMakeLists.txt includes this file to access all variables

### 2. Exported Variables

The following categories of variables are exported:

#### Source Files
- `ABS_cache_sources` - Cache-related source files
- `ABS_dataStructure_sources` - Data structure source files
- `ABS_traceReader_sources` - Trace reader source files
- `ABS_profiler_sources` - Profiler source files
- `ABS_utils_sources` - Utility source files
- `ABS_traceAnalyzer_sources` - Trace analyzer source files
- `ABS_mrcProfiler_sources` - MRC profiler source files

#### Project Metadata
- `LIBCACHESIM_VERSION` - Version information

#### Include Directories
- `libCacheSim_include_dir` - Main include directory
- `libCacheSim_binary_include_dir` - Binary include directory
- `GLib_INCLUDE_DIRS` - GLib include directories
- `XGBOOST_INCLUDE_DIR` - XGBoost include directory
- `LIGHTGBM_PATH` - LightGBM include directory
- `ZSTD_INCLUDE_DIR` - ZSTD include directory

#### Dependencies
- `dependency_libs` - Dependency libraries

#### Compiler Flags
- `LIBCACHESIM_C_FLAGS` - C compiler flags
- `LIBCACHESIM_CXX_FLAGS` - C++ compiler flags

#### Build Options
- `USE_HUGEPAGE` - Hugepage usage
- `ENABLE_TESTS` - Test enablement
- `ENABLE_GLCACHE` - GLCache enablement
- `SUPPORT_TTL` - TTL support
- `OPT_SUPPORT_ZSTD_TRACE` - ZSTD trace support
- `ENABLE_LRB` - LRB enablement
- `ENABLE_3L_CACHE` - 3L Cache enablement
- `LOG_LEVEL_LOWER` - Log level

## Usage

### In Main Project

The main project's CMakeLists.txt includes this export directory:

```cmake
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libCacheSim-python/export)
```

### In Python Binding

The Python binding's CMakeLists.txt imports the exported variables:

```cmake
set(PARENT_BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../build")
set(EXPORT_FILE "${PARENT_BUILD_DIR}/export_vars.cmake")

if(EXISTS "${EXPORT_FILE}")
    include("${EXPORT_FILE}")
    message(STATUS "Loaded variables from export_vars.cmake")
else()
    message(FATAL_ERROR "export_vars.cmake not found")
endif()
```