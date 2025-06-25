# ==============================================================================
# Cachesim
# ==============================================================================

set(cachesim_sources_c
    main.c
    cli_parser.c
    sim.c
    ../cli_reader_utils.c
)
add_executable(cachesim ${cachesim_sources_c})
target_include_directories(cachesim PRIVATE ${libCacheSim_include_dir} ${GLib_INCLUDE_DIRS} ../)
target_link_libraries(cachesim ${PROJECT_NAME})
install(TARGETS cachesim RUNTIME DESTINATION bin)
