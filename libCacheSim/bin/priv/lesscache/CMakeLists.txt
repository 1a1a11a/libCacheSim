# ==============================================================================
# LessCache
# ==============================================================================

set(lesscache_sources_c
    main.c
)

add_executable(lesscache ${lesscache_sources_c})
target_link_libraries(lesscache ${core_libs} ${dependency_libs})
