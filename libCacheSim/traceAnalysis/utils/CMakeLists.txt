#aux_source_directory(. DIR_LIB_SRCS)
#add_library (utils ${DIR_LIB_SRCS})
#target_link_libraries(utils traceReader)


set (SOURCE utils.cpp utilsSys.cpp threadPool.cpp)
add_library (utils ${SOURCE})
#target_link_libraries(utils traceReader)



