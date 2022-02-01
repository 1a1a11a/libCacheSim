set(sourceC
        MRU.c
        LRUv0.c
        Random.c
        LFUFast.c
        LFUDA.c
        ARC.c
        FIFO.c
        Clock.c
        LRU.c
        SLRU.c

        Optimal.c
        OptimalSize.c
        Hyperbolic.c
        LeCaR.c
        Cacheus.c
        FIFOMerge.c
        
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

if (ENABLE_L2CACHE)
    add_subdirectory(L2Cache)
    set(evictionLib ${evictionLib} L2Cache)
endif()


add_library(eviction INTERFACE)
target_link_libraries(eviction INTERFACE ${evictionLib})

