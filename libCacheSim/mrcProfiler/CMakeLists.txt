# ==============================================================================
# MRC Profiler Library
# ==============================================================================

# Collect all source files
set(mrcProfiler_sources_cpp
    mrcProfiler.cpp
)

add_library(mrcProfiler_lib OBJECT ${mrcProfiler_sources_cpp})

# Set C++ standard
set_target_properties(mrcProfiler_lib PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

# Set target-specific compiler flags
target_compile_options(mrcProfiler_lib PRIVATE
    ${LIBCACHESIM_CXX_FLAGS}
)

target_include_directories(mrcProfiler_lib PRIVATE
    ${GLib_INCLUDE_DIRS}
    ${libCacheSim_include_dir}
)

target_link_libraries(mrcProfiler_lib PUBLIC profiler_lib)
