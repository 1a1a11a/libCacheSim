# ==============================================================================
# Prefetch Algorithms Library
# ==============================================================================

set(prefetch_sources_c
    Mithril.c
    OBL.c
    PG.c
)

add_library(prefetch_lib_c ${prefetch_sources_c})

add_library(prefetch_lib INTERFACE)
target_link_libraries(prefetch_lib INTERFACE prefetch_lib_c)

set(cache_sources ${cache_sources} ${prefetch_sources_c} PARENT_SCOPE)
