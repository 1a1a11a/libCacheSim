aux_source_directory(. DIR_LIB_SRCS)
aux_source_directory(customizedReader DIR_LIB_SRCS2)
add_library (traceReader ${DIR_LIB_SRCS} ${DIR_LIB_SRCS2})
message("compile traceReader")
