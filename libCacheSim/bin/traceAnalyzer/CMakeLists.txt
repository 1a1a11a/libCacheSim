
add_executable(traceAnalyzer main.cpp cli_parser.cpp ../cli_reader_utils.c)
target_link_libraries(traceAnalyzer traceAnalyzerLib ${ALL_MODULES} ${LIBS} ${CMAKE_THREAD_LIBS_INIT} utils)
install(TARGETS traceAnalyzer RUNTIME DESTINATION bin)

set_target_properties(traceAnalyzer
        PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)
