set(source
        ARC.c
        FIFO.c
        LFU.c
        LFUDA.c
        LRU.c
        LRUv0.c
        MRU.c
        optimal.c
        Random.c

        slabLRC.c
        slabLRU.c
        slabObjLRU.c
        )

if (ENABLE_LLSC)
set(source ${source}
        logStructure/LLSC.c
        logStructure/learned.c
        logStructure/oracle.c
        )
endif()

#include_directories(${LIGHTGBM_INCLUDE_DIR})
add_library (evictionAlgo ${source})
target_link_libraries(evictionAlgo cache ${LIGHTGBM_LIB})


#file(GLOB src *.c)
#add_library (cacheAlgo ${src})
#target_link_libraries(cacheAlgo cache)
#set(LIB_SOURCE ${LIB_SOURCE} ${src} PARENT_SCOPE)
