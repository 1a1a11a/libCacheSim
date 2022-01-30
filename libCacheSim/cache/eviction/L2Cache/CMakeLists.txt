set(source
        L2Cache.c

        segment.c
        bucket.c

        segSel.c 
        eviction.c 

        oracle.c

        feature.c
        train.c 
        inference.c
        learned.c
        )

add_library(L2Cache ${source})
target_link_libraries(L2Cache ${LIGHTGBM_LIB})



