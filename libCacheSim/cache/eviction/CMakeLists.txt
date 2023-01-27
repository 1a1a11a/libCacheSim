set(sourceC
        MRU.c
        Random.c
        LFU.c
        LFUDA.c
        ARC.c
        FIFO.c
        LRU.c
        Clock.c
        SLRU.c
        SLRUv0.c
        WTinyLFUv0.c
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
        )

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/priv")
    set(sourceC ${sourceC} 
                priv/SFIFO_Merge.c
                priv/SFIFO_Reinsertion.c

                priv/LRU_Prob.c

                priv/MyClock.c

                priv/LPv1.c
                priv/LP_TwoQ.c
                priv/QDLPv1.c
                priv/QDLPv2.c

                priv/WTinyLFU.c

                priv/SFIFO.c
                priv/LP_SFIFO.c
                priv/QDLP.c
                )
endif()

set(sourceCPP
        cpp/LFU.cpp
        cpp/GDSF.cpp
        LHD/lhd.cpp
        LHD/LHDInterface.cpp
        )

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

