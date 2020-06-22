aux_source_directory(. DIR_LIB_SRCS)
add_library (profiler ${DIR_LIB_SRCS})
target_link_libraries(profiler traceReader)

#file(GLOB src *.c)
#add_library (profiler ${src})
#set(LIB_SOURCE ${LIB_SOURCE} ${src} PARENT_SCOPE)
