# ==============================================================================
# MRC Profiler Library
# ==============================================================================

# Collect all source files
set(mrcProfiler_sources_cpp
    mrcProfiler.cpp
)

add_library(mrcProfiler_lib ${mrcProfiler_sources_cpp})

# Set C++ standard
set_target_properties(mrcProfiler_lib PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

# Set target-specific compiler flags
target_compile_options(mrcProfiler_lib PRIVATE
    ${LIBCACHESIM_CXX_FLAGS}
    ${tcmalloc_flags}
)

target_include_directories(mrcProfiler_lib PRIVATE 
    ${GLib_INCLUDE_DIRS}
    ${libCacheSim_include_dir}
    ${libCacheSim_binary_include_dir}
)

# Link dependencies - profiler_lib already brings in cache_lib_c, cache_lib_cpp, traceReader_lib, utils_lib, dataStructure_lib and dependency_libs
target_link_libraries(mrcProfiler_lib PUBLIC profiler_lib)

set(mrcProfiler_sources ${mrcProfiler_sources_cpp} PARENT_SCOPE)
