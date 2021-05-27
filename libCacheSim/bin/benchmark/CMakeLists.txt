

add_executable(benchmark cacheAlgoPerf.c main.c)
target_link_libraries(benchmark cache evictionAlgo traceReader profiler dataStructure utils
        ${CMAKE_THREAD_LIBS_INIT} -ldl)


