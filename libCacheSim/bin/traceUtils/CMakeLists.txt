# ==============================================================================
# Trace Utils
# ==============================================================================

add_library(cliReaderLib ../cli_reader_utils.c)


set(tracePrint_sources_cpp
    tracePrintMain.cpp
    cli_parser.cpp
)
add_executable(tracePrint ${tracePrint_sources_cpp})
target_link_libraries(tracePrint cliReaderLib ${core_libs} ${dependency_libs})
set_target_properties(tracePrint
        PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        )

install(TARGETS tracePrint RUNTIME DESTINATION bin)

set(traceConv_sources_cpp
    traceConvMain.cpp
    traceConvOracleGeneral.cpp
    traceConvLCS.cpp
    cli_parser.cpp
    utils.cpp
)
add_executable(traceConv ${traceConv_sources_cpp})
target_link_libraries(traceConv cliReaderLib ${core_libs} ${dependency_libs})
set_target_properties(traceConv
        PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        )

install(TARGETS traceConv RUNTIME DESTINATION bin)

set(traceFilter_sources_cpp
    traceFilterMain.cpp
    cli_parser.cpp
)
add_executable(traceFilter ${traceFilter_sources_cpp})
target_link_libraries(traceFilter cliReaderLib ${core_libs} ${dependency_libs})
set_target_properties(traceFilter
        PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        )

install(TARGETS traceFilter RUNTIME DESTINATION bin)

