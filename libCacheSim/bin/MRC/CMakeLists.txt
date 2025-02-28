
add_executable(MRC main.c parser_shard.c SHARDS.c ../cli_reader_utils.c DataStructure/histogram.c DataStructure/splay_tuple.c DataStructure/splay.c parser_mini.c Miniatures.c)
target_link_libraries(MRC ${ALL_MODULES} ${LIBS} ${CMAKE_THREAD_LIBS_INIT} utils)
install(TARGETS MRC RUNTIME DESTINATION bin)

