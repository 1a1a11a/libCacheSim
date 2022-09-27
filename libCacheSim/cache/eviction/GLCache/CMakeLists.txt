set(source
        GLCache.c
        init.c
        
        segment.c
        bucket.c

        segSel.c 
        eviction.c 

        dataPrep.c
        train.c 
        inference.c
        )

add_library(GLCache ${source})
