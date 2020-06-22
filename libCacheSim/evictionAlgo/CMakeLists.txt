aux_source_directory(. DIR_LIB_SRCS)
add_library (evictionAlgo ${DIR_LIB_SRCS})
target_link_libraries(evictionAlgo cache)


#file(GLOB src *.c)
#add_library (cacheAlgo ${src})
#target_link_libraries(cacheAlgo cache)
#set(LIB_SOURCE ${LIB_SOURCE} ${src} PARENT_SCOPE)
