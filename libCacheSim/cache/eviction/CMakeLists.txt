set(sourceC
        MRU.c
        Random.c
        RandomTwo.c
        LFU.c
        LFUDA.c
        ARC.c
        FIFO.c
        LRU.c
        Clock.c
        ClockPro.c
        SLRU.c
        SLRUv0.c
        CR_LFU.c
        SR_LRU.c
        Belady.c
        BeladySize.c
        Hyperbolic.c
        LeCaR.c
        Cacheus.c
        TwoQ.c
        ARCv0.c
        LRUv0.c
        LeCaRv0.c
        LIRS.c
        WTinyLFU.c
        Size.c
        LRUProb.c
        QDLP.c
        nop.c

        FIFO_Merge.c
        FIFO_Reinsertion.c

        S3FIFO.c
        S3FIFOv0.c
        S3FIFOd.c

        other/flashProb.c
        other/S3LRU.c

        Sieve.c

        RandomLRU.c
)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/priv")
    set(sourceC ${sourceC} 

                priv/QDLPv0.c


                priv/S3FIFOdv2.c

                priv/myMQv1.c

                priv/MClock.c
                )
endif()

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/fifo")
        set(sourceC ${sourceC} 
        fifo/LP_SFIFO.c
        fifo/LP_TwoQ.c
        fifo/LP_ARC.c
        fifo/SFIFO.c
        fifo/SFIFOv0.c
        )
endif()

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/belady")
        set(sourceC ${sourceC} 
        belady/Sieve_Belady.c
        belady/LRU_Belady.c
        belady/FIFO_Belady.c
        )
endif()

set(sourceCPP
        cpp/LFU.cpp
        cpp/GDSF.cpp
        LHD/lhd.cpp
        LHD/LHD_Interface.cpp
        )

if (ENABLE_LRB)
    set (sourceCPP ${sourceCPP} 
        LRB/lrb.cpp
        LRB/LRB_Interface.cpp
        )
endif()

if (ENABLE_3L_CACHE)
    set (sourceCPP ${sourceCPP} 
        3LCache/ThreeLCache_Interface.cpp
        3LCache/ThreeLCache.cpp
        )
endif()

if (ENABLE_GLCACHE)
    set(sourceC 
        ${sourceC} 
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


add_library (evictionC ${sourceC})
target_link_libraries(evictionC cachelib dataStructure utils)
add_library (evictionCPP ${sourceCPP})
target_link_libraries(evictionCPP cachelib utils)
set_target_properties(evictionCPP
        PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        )

# set (evictionLib
#         evictionC
#         evictionCPP
#         )


        
# add_library(eviction INTERFACE)
# target_link_libraries(eviction INTERFACE ${evictionLib})

