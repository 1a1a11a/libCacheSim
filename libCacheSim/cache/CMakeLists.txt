aux_source_directory(. DIR_LIB_SRCS)
add_library(cache ${DIR_LIB_SRCS})
target_link_libraries(cache dataStructure -ldl)



#file(GLOB src *.c)
#add_library (cache ${src})
#target_link_libraries(cache dataStructure -ldl)
#set(LIB_SOURCE ${LIB_SOURCE} ${src} PARENT_SCOPE)

