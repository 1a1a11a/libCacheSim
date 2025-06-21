# ==============================================================================
# Profiler Library
# ==============================================================================

set(profiler_sources_c
    profilerLRU.c
    simulator.c
    dist.c
)

add_library(profiler_lib ${profiler_sources_c})

# Set target-specific compiler flags
target_compile_options(profiler_lib PRIVATE
    ${LIBCACHESIM_C_FLAGS}
    ${tcmalloc_flags}
)

target_include_directories(profiler_lib PRIVATE 
    ${GLib_INCLUDE_DIRS}
    ${libCacheSim_include_dir}
    ${libCacheSim_binary_include_dir}
)

target_link_libraries(profiler_lib cache_lib_c cache_lib_cpp traceReader_lib utils_lib dataStructure_lib ${dependency_libs})

set(profiler_sources ${profiler_sources_c} PARENT_SCOPE)
