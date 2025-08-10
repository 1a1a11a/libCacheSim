# ==============================================================================
# Trace Reader Library
# ==============================================================================

set(traceReader_sources_c
    generalReader/binary.c 
    generalReader/csv.c 
    generalReader/txt.c 
    generalReader/libcsv.c
    customizedReader/lcs.c
    reader.c
    sampling/spatial.c
    sampling/temporal.c
    )

if (OPT_SUPPORT_ZSTD_TRACE)
    set(traceReader_sources_c ${traceReader_sources_c} generalReader/zstdReader.c)
endif (OPT_SUPPORT_ZSTD_TRACE)

add_library(traceReader_lib OBJECT ${traceReader_sources_c})
# Set target-specific compiler flags
target_compile_options(traceReader_lib PRIVATE
    ${LIBCACHESIM_C_FLAGS}
)

target_include_directories(traceReader_lib PRIVATE
    ${GLib_INCLUDE_DIRS}
    ${libCacheSim_include_dir}
)

target_link_libraries(traceReader_lib PRIVATE utils_lib ${ZSTD_LIBRARIES})
