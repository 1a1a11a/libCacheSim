file(GLOB lhdsrc *.cpp)
add_library (LHD ${lhdsrc})
target_link_libraries(LHD)

set_target_properties(LHD
        PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        )

