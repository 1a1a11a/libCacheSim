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

add_library(traceReader_lib ${traceReader_sources_c})
# Set target-specific compiler flags
target_compile_options(traceReader_lib PRIVATE
    ${LIBCACHESIM_C_FLAGS}
    ${tcmalloc_flags}
)

target_include_directories(traceReader_lib PRIVATE 
    ${GLib_INCLUDE_DIRS}
    ${libCacheSim_include_dir}
    ${libCacheSim_binary_include_dir}
)

target_link_libraries(traceReader_lib utils_lib ${ZSTD_LIBRARIES})

set(traceReader_sources ${traceReader_sources_c} PARENT_SCOPE)


