#include_directories(include)
#include_directories(hash)
#include_directories(hashtable)
add_subdirectory(hash)
add_subdirectory(hashtable)

set(source
        pqueue.c
        splay.c
        bloom.c
        hash/murmur3.c
        hashtable/chainedHashtable.c
        hashtable/chainedHashTableV2.c
        )
add_library (dataStructure ${source})
#target_link_libraries(dataStructure ds_hash ds_hashtable)


