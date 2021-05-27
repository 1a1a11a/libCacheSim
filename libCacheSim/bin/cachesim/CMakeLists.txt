

add_executable(cachesim main.c cli.c)
target_link_libraries(cachesim cache ${evictionAlgos} LHD traceReader profiler dataStructure utils)

