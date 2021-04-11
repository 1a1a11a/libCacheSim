
add_library(cache cache.c cache_obj.c plugin.c)
target_link_libraries(cache dataStructure -ldl)



#file(GLOB src *.c)
#add_library (cache ${src})
#target_link_libraries(cache dataStructure -ldl)
#set(LIB_SOURCE ${LIB_SOURCE} ${src} PARENT_SCOPE)

