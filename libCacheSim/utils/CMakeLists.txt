aux_source_directory(. DIR_LIB_SRCS)
add_library (utils ${DIR_LIB_SRCS})
target_link_libraries(utils traceReader)




