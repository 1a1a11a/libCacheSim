aux_source_directory(. DIR_LIB_SRCS)
add_library (utils ${DIR_LIB_SRCS} plugin.c ../include/mimircache/plugin.h)
