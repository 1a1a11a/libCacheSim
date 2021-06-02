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
        Optimal.c
        Hyperbolic.c
        LeCaR.c
        Cacheus.c

        slabLRC.c
        slabLRU.c
        slabObjLRU.c
        )

set (sourceCPP
        cpp/LFU.cpp
        cpp/GDSF.cpp)

set (evictionLib
        evictionC
        evictionCPP
        LHD)

if (ENABLE_LLSC)
    add_subdirectory(logStructure)
    set(evictionLib ${evictionLib} L2Cache)
endif()

add_subdirectory(LHD)

add_library (evictionC ${sourceC})
target_link_libraries(evictionC cache dataStructure)
add_library (evictionCPP ${sourceCPP})
target_link_libraries(evictionC cache)
set_target_properties(evictionCPP
        PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        )


add_library(eviction INTERFACE)
target_link_libraries(eviction INTERFACE ${evictionLib})

