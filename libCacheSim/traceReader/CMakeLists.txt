aux_source_directory(. DIR_LIB_SRCS)
aux_source_directory(customizedReader DIR_LIB_SRCS2)
add_library (traceReader ${DIR_LIB_SRCS} ${DIR_LIB_SRCS2})

#file(GLOB src *.c)
#add_library (traceReader ${src})
#set(LIB_SOURCE ${LIB_SOURCE} ${src} PARENT_SCOPE)
