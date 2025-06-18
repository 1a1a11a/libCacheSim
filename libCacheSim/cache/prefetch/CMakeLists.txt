# ==============================================================================
# Prefetch Algorithms Library
# ==============================================================================

set(PREFETCH_C_SOURCES
    Mithril.c
    OBL.c
    PG.c
)

add_library(prefetchC ${PREFETCH_C_SOURCES})
target_link_libraries(prefetchC cachelib utils)
target_include_directories(prefetchC PUBLIC 
    ${GLib_INCLUDE_DIRS}
    ${libCacheSim_include_dir}
    ${libCacheSim_binary_include_dir}
)

add_library(prefetch INTERFACE)
target_link_libraries(prefetch INTERFACE prefetchC)


