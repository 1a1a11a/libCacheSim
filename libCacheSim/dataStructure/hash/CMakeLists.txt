# aux_source_directory(. DIR_LIB_SRCS)
# add_library (ds_hash ${DIR_LIB_SRCS})

add_library (ds_hash murmur3.c)

