set(eviction_sources_c
        ARC.c
        ARCv0.c         # an inefficient version but easier to understand
        Belady.c
        BeladySize.c
        CAR.c
        Cacheus.c
        Clock.c
        ClockPro.c
        FIFO.c
        Hyperbolic.c
        LeCaR.c
        LeCaRv0.c       # an inefficient version but easier to understand
        LFU.c
        LFUDA.c
        LIRS.c
        LRU.c
        LRUProb.c       # a probabilistic version of LRU
        LRUv0.c         # an inefficient version but easier to understand
        MRU.c
        nop.c
        QDLP.c
        Random.c
        RandomLRU.c
        RandomTwo.c
        S3FIFO.c
        Sieve.c
        Size.c
        SLRU.c
        SLRUv0.c        # an inefficient version but easier to understand
        TwoQ.c
        WTinyLFU.c

        # these are sub-algorithms used by other algorithms
        CR_LFU.c
        SR_LRU.c

        # these are algorithms used for comparison
        FIFO_Merge.c
        FIFO_Reinsertion.c
        S3FIFOv0.c
        S3FIFOd.c
        other/flashProb.c
        other/S3LRU.c


)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/fifo")
        set(eviction_sources_c ${eviction_sources_c} 
        fifo/LP_SFIFO.c
        fifo/LP_TwoQ.c
        fifo/LP_ARC.c
        fifo/SFIFO.c
        fifo/SFIFOv0.c
        )
endif()

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/belady")
        set(eviction_sources_c ${eviction_sources_c} 
        belady/Sieve_Belady.c
        belady/LRU_Belady.c
        belady/FIFO_Belady.c
        )
endif()

set(eviction_sources_cpp
        cpp/LFU.cpp
        cpp/GDSF.cpp
        LHD/lhd.cpp
        LHD/LHD_Interface.cpp
        )

if (ENABLE_LRB)
    set (eviction_sources_cpp ${eviction_sources_cpp} 
        LRB/lrb.cpp
        LRB/LRB_Interface.cpp
        )
endif()

if (ENABLE_3L_CACHE)
    set (eviction_sources_cpp ${eviction_sources_cpp} 
        3LCache/ThreeLCache_Interface.cpp
        3LCache/ThreeLCache.cpp
        )
endif()

if (ENABLE_GLCACHE)
    set(eviction_sources_c 
        ${eviction_sources_c} 
        GLCache/bucket.c
        GLCache/dataPrep.c
        GLCache/eviction.c
        GLCache/GLCache.c
        GLCache/inference.c
        GLCache/init.c
        GLCache/segment.c
        GLCache/segSel.c
        GLCache/train.c
        )
endif()


add_library (eviction_lib_c ${eviction_sources_c})
target_link_libraries(eviction_lib_c cachelib dataStructure utils)
add_library (eviction_lib_cpp ${eviction_sources_cpp})
target_link_libraries(eviction_lib_cpp cachelib utils)
set_target_properties(eviction_lib_cpp
        PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        )

add_library(eviction_lib INTERFACE)
target_link_libraries(eviction_lib INTERFACE eviction_lib_c eviction_lib_cpp)

set(cache_sources ${cache_sources} ${eviction_sources_c} ${eviction_sources_cpp} PARENT_SCOPE)

