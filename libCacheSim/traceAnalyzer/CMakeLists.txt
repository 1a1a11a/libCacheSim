# ==============================================================================
# Trace Analyzer Library
# ==============================================================================


# Collect all analyzer source files
set(traceAnalyzer_sources_cpp
    utils/threadPool.cpp
    utils/utils.cpp 
    utils/utilsSys.cpp 

    accessPattern.cpp
    analyzer.cpp
    popularity.cpp
    popularityDecay.cpp
    reqRate.cpp
    reuse.cpp
    size.cpp
    ttl.cpp

    experimental/createFutureReuseCCDF.cpp
    experimental/lifetime.cpp
    experimental/probAtAge.cpp
    experimental/sizeChange.cpp
)

add_library(traceAnalyzer_lib ${traceAnalyzer_sources_cpp})
target_link_libraries(traceAnalyzer_lib ${dependency_libs})
set_target_properties(traceAnalyzer_lib PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

set(traceAnalyzer_sources ${traceAnalyzer_sources_cpp} PARENT_SCOPE)
