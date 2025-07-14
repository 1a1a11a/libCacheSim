# Python Binding Export System

Build system bridge for sharing CMake variables between the main libCacheSim project and Python binding.

## Purpose

The `export/CMakeLists.txt` exports all necessary build variables (source files, include directories, compiler flags, etc.) from the main project to the Python binding, enabling consistent builds without duplicating configuration.

## How It Works

1. **Export**: Main project writes variables to `export_vars.cmake`
2. **Import**: Python binding includes this file during CMake configuration
3. **Build**: Python binding uses shared variables for consistent compilation

## Key Exported Variables

### Source Files
- Cache algorithms, data structures, trace readers
- Profilers, utilities, analyzers

### Build Configuration
- Include directories (main, GLib, ZSTD, XGBoost, LightGBM)
- Compiler flags (C/C++)
- Dependency libraries
- Build options (hugepage, tests, optional features)

## Usage

**Main Project** (`CMakeLists.txt`):
```cmake
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libCacheSim-python/export)
```

**Python Binding** (`libCacheSim-python/CMakeLists.txt`):
```cmake
set(EXPORT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../build/export_vars.cmake")
include("${EXPORT_FILE}")
```

## For Developers

This system ensures the Python binding automatically picks up changes to:
- New source files added to the main project
- Updated compiler flags or dependencies
- Modified build options

No manual synchronization needed between main project and Python binding builds.
