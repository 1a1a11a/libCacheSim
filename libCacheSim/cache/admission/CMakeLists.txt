# ==============================================================================
# Cache Admission Libraries
# ==============================================================================

# C admission algorithms
set(ADMISSION_C_SOURCES
    bloomfilter.c
    prob.c
    size.c
    sizeProbabilistic.c
)

add_library(admissionC ${ADMISSION_C_SOURCES})
target_link_libraries(admissionC cachelib dataStructure utils)
target_include_directories(admissionC PUBLIC 
    ${GLib_INCLUDE_DIRS}
    ${libCacheSim_include_dir}
    ${libCacheSim_binary_include_dir}
)

# C++ admission algorithms
set(ADMISSION_CPP_SOURCES
    adaptsize/adaptsize.cpp
    adaptsize/adaptsize_interface.cpp
)

add_library(admissionCpp ${ADMISSION_CPP_SOURCES})
target_link_libraries(admissionCpp cachelib utils)
target_include_directories(admissionCpp PUBLIC 
    ${GLib_INCLUDE_DIRS}
    ${libCacheSim_include_dir}
    ${libCacheSim_binary_include_dir}
)

# Set C++ standard for C++ library
set_target_properties(admissionCpp PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)