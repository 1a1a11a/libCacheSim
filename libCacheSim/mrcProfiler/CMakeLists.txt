
file(GLOB source *.cpp)

add_library (mrcProfilerLib ${source} ${source2})

set_target_properties(mrcProfilerLib
        PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)
