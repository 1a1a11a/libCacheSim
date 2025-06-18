# ==============================================================================
# Cache Library
# ==============================================================================

include_directories(admission)
include_directories(eviction)
include_directories(prefetch)

# Add subdirectories
add_subdirectory(admission)
add_subdirectory(eviction)
add_subdirectory(prefetch)

# Main cache library
set(CACHE_SOURCES
    cache.c
    plugin.c
)

add_library(cachelib ${CACHE_SOURCES})
target_link_libraries(cachelib dataStructure)
target_include_directories(cachelib PUBLIC 
    ${GLib_INCLUDE_DIRS}
    ${libCacheSim_include_dir}
    ${libCacheSim_binary_include_dir}
)

# Set PIC and export symbols
target_compile_options(cachelib PRIVATE -fPIC)
target_link_options(cachelib PRIVATE -Wl,--export-dynamic)
