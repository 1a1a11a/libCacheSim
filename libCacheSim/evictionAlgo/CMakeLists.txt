set(sourceC
        ARC.c
        FIFO.c
        LFU.c
        LFUDA.c
        LRU.c
        LRUv0.c
        MRU.c
        Optimal.c
        Random.c

        slabLRC.c
        slabLRU.c
        slabObjLRU.c
        )

set (sourceCPP
        cpp/GDSF.cpp)


if (ENABLE_LLSC)
    add_subdirectory(logStructure)
endif()

add_subdirectory(LHD)

add_library (evictionAlgoC ${sourceC})
add_library (evictionAlgoCPP ${sourceCPP})

set_target_properties(evictionAlgoCPP
        PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        )

add_library(evictionAlgos INTERFACE)
target_link_libraries(evictionAlgos INTERFACE evictionAlgoC evictionAlgoCPP LHD)

