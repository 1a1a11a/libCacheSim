set(sourceC
        MRU.c
        Random.c
        LFU.c
        LFUDA.c
        ARC.c
        FIFO.c
        Clock.c
        LRU.c
        SLRU.c
        CR_LFU.c
        SR_LRU.c
        Belady.c
        BeladySize.c
        Hyperbolic.c
        LeCaR.c
        Cacheus.c
        FIFO_Merge.c
        FIFO_Reinsertion.c
        SFIFO.c

        LRU_Belady.c
        SFIFO_Belady.c

        LRUv0.c
        LeCaRv0.c
        )

set (sourceCPP
        cpp/LFU.cpp
        cpp/GDSF.cpp)

add_subdirectory(LHD)

add_library (evictionC ${sourceC})
target_link_libraries(evictionC cachelib dataStructure utils)
add_library (evictionCPP ${sourceCPP})
target_link_libraries(evictionC cachelib utils)
set_target_properties(evictionCPP
        PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        )

set (evictionLib
        evictionC
        evictionCPP
        LHD)

if (ENABLE_GLCACHE)
    add_subdirectory(GLCache)
    set(evictionLib ${evictionLib} GLCache)
endif()


add_library(eviction INTERFACE)
target_link_libraries(eviction INTERFACE ${evictionLib})

