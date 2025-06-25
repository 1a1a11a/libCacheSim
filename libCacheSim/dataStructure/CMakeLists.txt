# ==============================================================================
# Data Structure Library
# ==============================================================================

set(dataStructure_sources_c
    pqueue.c
    splay.c
    bloom.c
    minimalIncrementCBF.c
    histogram.c
    splay_tuple.c
    hash/murmur3.c
    hashtable/chainedHashtable.c
    hashtable/chainedHashTableV2.c
)

add_library(dataStructure_lib OBJECT ${dataStructure_sources_c})

target_compile_options(dataStructure_lib PRIVATE
    ${LIBCACHESIM_C_FLAGS}
)

target_include_directories(dataStructure_lib PRIVATE
    ${GLib_INCLUDE_DIRS}
    ${libCacheSim_include_dir}
)
