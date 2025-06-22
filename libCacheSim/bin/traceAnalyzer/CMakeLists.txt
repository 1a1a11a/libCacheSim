# ==============================================================================
# Trace Analyzer
# ==============================================================================

set(traceAnalyzer_sources_cpp
    main.cpp
    cli_parser.cpp
    ../cli_reader_utils.c
)
add_executable(traceAnalyzer ${traceAnalyzer_sources_cpp})
target_link_libraries(traceAnalyzer traceAnalyzer_lib ${core_libs} ${dependency_libs})
install(TARGETS traceAnalyzer RUNTIME DESTINATION bin)

set_target_properties(traceAnalyzer
        PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)
