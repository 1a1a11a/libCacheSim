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

add_library(traceAnalyzer_lib OBJECT ${traceAnalyzer_sources_cpp})
target_include_directories(traceAnalyzer_lib PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${libCacheSim_include_dir}
    ${GLib_INCLUDE_DIRS}
)
target_compile_options(traceAnalyzer_lib PRIVATE
    ${LIBCACHESIM_CXX_FLAGS}
)
target_link_libraries(traceAnalyzer_lib PRIVATE ${dependency_libs})
set_target_properties(traceAnalyzer_lib PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
