# ==============================================================================
# Trace Analyzer
# ==============================================================================

set(traceAnalyzer_sources_cpp
    main.cpp
    cli_parser.cpp
    ../cli_reader_utils.c
)
add_executable(traceAnalyzer ${traceAnalyzer_sources_cpp})
target_include_directories(traceAnalyzer PRIVATE ${libCacheSim_include_dir} ${GLib_INCLUDE_DIRS})
target_link_libraries(traceAnalyzer ${PROJECT_NAME})
install(TARGETS traceAnalyzer RUNTIME DESTINATION bin)

set_target_properties(traceAnalyzer
        PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)
