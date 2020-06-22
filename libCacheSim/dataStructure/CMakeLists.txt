#include_directories(include)
#include_directories(hash)
#include_directories(hashtable)
add_subdirectory(hash)
add_subdirectory(hashtable)

#aux_source_directory(. DIR_LIB_SRCS)
set(DIR_LIB_SRCS
        pqueue.c
        splay.c
        hash/murmur3.c
        hashtable/chainedHashtable.c
        hashtable/chainedHashTableV2.c)
add_library (dataStructure ${DIR_LIB_SRCS})
#target_link_libraries(dataStructure ds_hash ds_hashtable)


