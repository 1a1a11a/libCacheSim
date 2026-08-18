# ==============================================================================
# Trace Utils
# ==============================================================================

add_library(cliReaderLib ../cli_reader_utils.c)
target_include_directories(cliReaderLib PRIVATE
    ${libCacheSim_include_dir}
    ${GLib_INCLUDE_DIRS}
)
target_compile_options(cliReaderLib PRIVATE ${LIBCACHESIM_C_FLAGS})
target_link_libraries(cliReaderLib PRIVATE ${PROJECT_NAME})

set(tracePrint_sources_cpp
    tracePrintMain.cpp
    cli_parser.cpp
)
add_executable(tracePrint ${tracePrint_sources_cpp})
target_include_directories(tracePrint PRIVATE ${libCacheSim_include_dir} ${GLib_INCLUDE_DIRS})
target_link_libraries(tracePrint ${PROJECT_NAME} cliReaderLib)
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
target_include_directories(traceConv PRIVATE ${libCacheSim_include_dir} ${GLib_INCLUDE_DIRS})
target_link_libraries(traceConv ${PROJECT_NAME} cliReaderLib)
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
target_include_directories(traceFilter PRIVATE ${libCacheSim_include_dir} ${GLib_INCLUDE_DIRS})
target_link_libraries(traceFilter ${PROJECT_NAME} cliReaderLib)
set_target_properties(traceFilter
        PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        )

install(TARGETS traceFilter RUNTIME DESTINATION bin)

# ==============================================================================
# traceConvLLM -- qwen-style JSONL -> lcsllm binary
# ==============================================================================
# Links prefixsim_lib to reuse its JSONL parser, prefix-hash folding and
# next-access annotation, so the converter and the simulator cannot disagree
# about what a trace means.
add_executable(traceConvLLM traceConvLLM.cpp)
target_compile_options(traceConvLLM PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:${LIBCACHESIM_CXX_FLAGS}>
)
target_include_directories(traceConvLLM PRIVATE ${libCacheSim_include_dir} ${GLib_INCLUDE_DIRS})
target_link_libraries(traceConvLLM prefixsim_lib)
set_target_properties(traceConvLLM
        PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        )

install(TARGETS traceConvLLM RUNTIME DESTINATION bin)
