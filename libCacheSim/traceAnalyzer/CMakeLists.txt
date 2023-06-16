
file(GLOB source *.cpp)
file(GLOB source2 experimental/*.cpp)

add_library (traceAnalyzerLib ${source} ${source2})

set_target_properties(traceAnalyzerLib
        PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)

