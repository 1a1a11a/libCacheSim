aux_source_directory(. DIR_LIB_SRCS)
add_library (profiler ${DIR_LIB_SRCS})
target_link_libraries(profiler traceReader)