# ==============================================================================
# MRC Profiler Library
# ==============================================================================

# Collect all source files
file(GLOB MRC_SOURCES *.cpp)

add_library(mrcProfilerLib ${MRC_SOURCES})

# Set C++ standard
set_target_properties(mrcProfilerLib PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

# Link dependencies and set include paths  
target_link_libraries(mrcProfilerLib utils)
target_include_directories(mrcProfilerLib PUBLIC 
    ${GLib_INCLUDE_DIRS}
    ${libCacheSim_include_dir}
    ${libCacheSim_binary_include_dir}
)
