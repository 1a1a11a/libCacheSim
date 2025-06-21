# ==============================================================================
# DistUtil
# ==============================================================================

set(distUtil_sources_c
    main.c
    cli.c
    ../cli_reader_utils.c
)
add_executable(distUtil ${distUtil_sources_c})
target_link_libraries(distUtil ${all_modules} ${dependency_libs} utils_lib)
install(TARGETS cachesim RUNTIME DESTINATION bin)
