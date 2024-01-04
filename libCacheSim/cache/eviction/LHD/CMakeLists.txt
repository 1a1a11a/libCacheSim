file(GLOB src *.cpp)
add_library (LHD ${src})

set_property(LHD CXX_STANDARD 17)
