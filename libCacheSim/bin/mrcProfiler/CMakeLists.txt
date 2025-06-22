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
    ${tcmalloc_flags}
)

target_link_libraries(mrcProfiler mrcProfiler_lib m dl)

# target_link_options(mrcProfiler PRIVATE "-Wl,--export-dynamic")  # Not supported on macOS
install(TARGETS mrcProfiler RUNTIME DESTINATION bin)

set_target_properties(mrcProfiler
        PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)
