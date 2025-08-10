# ==============================================================================
# MRC Profiler
# ==============================================================================

set(mrcProfiler_sources_cpp
    main.cpp
    cli_parser.cpp
    ../cli_reader_utils.c
)
add_executable(mrcProfiler ${mrcProfiler_sources_cpp})

# Set target-specific compiler flags for mixed C/C++ sources
target_compile_options(mrcProfiler PRIVATE
    $<$<COMPILE_LANGUAGE:C>:${LIBCACHESIM_C_FLAGS}>
    $<$<COMPILE_LANGUAGE:CXX>:${LIBCACHESIM_CXX_FLAGS}>
)
target_include_directories(mrcProfiler PRIVATE ${libCacheSim_include_dir} ${GLib_INCLUDE_DIRS})
target_link_libraries(mrcProfiler ${PROJECT_NAME})

set_target_properties(mrcProfiler
        PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)

install(TARGETS mrcProfiler RUNTIME DESTINATION bin)
