add_subdirectory(hash)
set(source
        pqueue.c
        splay.c
        bloom.c
        minimalIncrementCBF.c
        hash/murmur3.c
        hashtable/chainedHashtable.c
        hashtable/chainedHashTableV2.c
        )
add_library (dataStructure ${source})

