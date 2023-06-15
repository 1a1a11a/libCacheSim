
set(source analysis.cpp)
add_library (analysisLib ${source})

set_target_properties(analysisLib
        PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)

