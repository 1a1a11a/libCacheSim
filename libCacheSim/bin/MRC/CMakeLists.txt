# ==============================================================================
# MRC
# ==============================================================================

set(mrc_sources_c
    main.c
    parser_shard.c
    SHARDS.c
    ../cli_reader_utils.c
    ../../dataStructure/histogram.c
    ../../dataStructure/splay_tuple.c
    ../../dataStructure/splay.c
    parser_mini.c
    Miniatures.c
)

add_executable(MRC ${mrc_sources_c})
target_include_directories(MRC PRIVATE ${libCacheSim_include_dir} ${GLib_INCLUDE_DIRS})
target_link_libraries(MRC ${PROJECT_NAME})
install(TARGETS MRC RUNTIME DESTINATION bin)
