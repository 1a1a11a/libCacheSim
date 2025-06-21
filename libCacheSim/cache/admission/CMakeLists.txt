# ==============================================================================
# Cache Admission Libraries
# ==============================================================================

include_directories(${libCacheSim_include_dir})

# C admission algorithms
set(admission_sources_c
    bloomfilter.c
    prob.c
    size.c
    sizeProbabilistic.c
)
# add_library(admission_lib_c ${admission_sources_c})

# C++ admission algorithms
set(admission_sources_cpp
    adaptsize/adaptsize.cpp
    adaptsize/adaptsize_interface.cpp
)
add_library(admission_lib_cpp ${admission_sources_cpp})
# Set C++ standard for C++ library
set_target_properties(admission_lib_cpp PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

set(cache_sources ${cache_sources} ${admission_sources_c} ${admission_sources_cpp} PARENT_SCOPE)
