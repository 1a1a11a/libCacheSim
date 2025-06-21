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
target_link_libraries(cachesim ${all_modules} ${dependency_libs})
target_link_options(cachesim PRIVATE "-Wl,--export-dynamic")
install(TARGETS cachesim RUNTIME DESTINATION bin)
