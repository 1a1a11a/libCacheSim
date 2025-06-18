# ==============================================================================
# Trace Analyzer Library
# ==============================================================================

# Include utility subdirectory
add_subdirectory(utils)

# Collect all analyzer source files
file(GLOB ANALYZER_SOURCES *.cpp)
file(GLOB EXPERIMENTAL_SOURCES experimental/*.cpp)

add_library(traceAnalyzerLib ${ANALYZER_SOURCES} ${EXPERIMENTAL_SOURCES})

# Set C++ standard
set_target_properties(traceAnalyzerLib PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

# Link dependencies and set include paths
target_link_libraries(traceAnalyzerLib traceAnalyzerUtils)
target_include_directories(traceAnalyzerLib PUBLIC 
    ${GLib_INCLUDE_DIRS}
    ${libCacheSim_include_dir}
    ${libCacheSim_binary_include_dir}
)

