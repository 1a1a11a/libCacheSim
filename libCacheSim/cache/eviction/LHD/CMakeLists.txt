file(GLOB src *.cpp)
add_library (LHD ${src})

set_target_properties(LHD
        PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        )


