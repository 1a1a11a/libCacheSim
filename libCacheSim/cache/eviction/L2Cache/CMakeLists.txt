set(source
        L2Cache.c

        segment.c
        bucket.c

        segSel.c 
        eviction.c 

        feature.c
        dataPrep.c
        train.c 
        inference.c
        )

add_library(L2Cache ${source})


add_executable(test_xgboost testXGBoost.c)
