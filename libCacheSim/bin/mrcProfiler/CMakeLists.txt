
add_executable(mrcProfiler main.cpp cli_parser.cpp ../cli_reader_utils.c)
target_link_libraries(mrcProfiler mrcProfilerLib m zstd dl pthread 
                -Wl,--whole-archive 
                libCacheSim 
                -Wl,--no-whole-archive
                ${ALL_MODULES} ${LIBS} ${CMAKE_THREAD_LIBS_INIT} utils )

target_link_options(mrcProfiler PRIVATE "-Wl,--export-dynamic")
install(TARGETS mrcProfiler RUNTIME DESTINATION bin)

set_target_properties(mrcProfiler
        PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)
