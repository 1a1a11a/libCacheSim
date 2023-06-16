
set(source analyzer.cpp)
add_library (traceAnalyzerLib ${source})

set_target_properties(traceAnalyzerLib
        PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)

