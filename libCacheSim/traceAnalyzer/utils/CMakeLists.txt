# ==============================================================================
# Trace Analyzer Utilities
# ==============================================================================

set(ANALYZER_UTILS_SOURCES 
    utils.cpp 
    utilsSys.cpp 
    threadPool.cpp
)

add_library(traceAnalyzerUtils ${ANALYZER_UTILS_SOURCES})
target_include_directories(traceAnalyzerUtils PUBLIC 
    ${GLib_INCLUDE_DIRS}
    ${libCacheSim_include_dir}
    ${libCacheSim_binary_include_dir}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Set C++ standard
set_target_properties(traceAnalyzerUtils PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)



