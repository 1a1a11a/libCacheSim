# ==============================================================================
# DistUtil
# ==============================================================================

set(distUtil_sources_c
    main.c
    cli.c
    ../cli_reader_utils.c
)
add_executable(distUtil ${distUtil_sources_c})
target_include_directories(distUtil PRIVATE ${libCacheSim_include_dir} ${GLib_INCLUDE_DIRS})
target_link_libraries(distUtil ${PROJECT_NAME})
install(TARGETS distUtil RUNTIME DESTINATION bin)
