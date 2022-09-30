
set(source 
    generalReader/binary.c 
    generalReader/csv.c 
    generalReader/txt.c 
    generalReader/libcsv.c 
    reader.c)

if (SUPPORT_ZSTD_TRACE)
    set(source ${source} generalReader/zstdReader.c)
endif (SUPPORT_ZSTD_TRACE)

add_library(traceReader ${source})



