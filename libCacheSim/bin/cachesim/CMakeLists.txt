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
target_include_directories(cachesim PRIVATE ../)
# Link just profiler_lib which now provides all needed libraries transitively
target_link_libraries(cachesim profiler_lib)
# target_link_options(cachesim PRIVATE "-Wl,--export-dynamic")  # Not supported on macOS
install(TARGETS cachesim RUNTIME DESTINATION bin)
