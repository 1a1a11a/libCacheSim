# ==============================================================================
# Data Structure Library
# ==============================================================================

add_subdirectory(hash)

set(DATASTRUCTURE_SOURCES
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

add_library(dataStructure ${DATASTRUCTURE_SOURCES})
target_include_directories(dataStructure PUBLIC 
    ${GLib_INCLUDE_DIRS}
    ${libCacheSim_include_dir}
    ${libCacheSim_binary_include_dir}
)

