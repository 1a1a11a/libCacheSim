
set(source 
    generalReader/binary.c 
    generalReader/csv.c 
    generalReader/txt.c 
    generalReader/libcsv.c
    generalReader/lcs.c
    reader.c
    sampling/spatial.c
    sampling/temporal.c
    )

if (SUPPORT_ZSTD_TRACE)
    set(source ${source} generalReader/zstdReader.c)
endif (SUPPORT_ZSTD_TRACE)

add_library(traceReader ${source})


