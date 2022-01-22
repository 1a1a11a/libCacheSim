set(source
        L2Cache.c
        learned.c
        oracle.c
        )

add_library(L2Cache ${source})
target_link_libraries(L2Cache ${LIGHTGBM_LIB})


