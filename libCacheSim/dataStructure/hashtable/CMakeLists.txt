aux_source_directory(. DIR_LIB_SRCS)
add_library (ds_hashtable ${DIR_LIB_SRCS})
target_link_libraries(ds_hashtable ds_hash)


