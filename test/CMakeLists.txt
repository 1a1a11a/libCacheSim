


add_executable(testReader ../test/test_traceReader.c)
target_link_libraries(testReader cache cacheAlgo traceReader profiler dataStructure traceStat utils m ${LIBS} ${CMAKE_THREAD_LIBS_INIT} -lpthread -ldl)

add_executable(testDistUtils distInternal.c)
target_link_libraries(testDistUtils cache cacheAlgo traceReader profiler dataStructure traceStat utils m ${LIBS} ${CMAKE_THREAD_LIBS_INIT} -lpthread -ldl)

add_executable(testProfilerLRU ../test/test_profilerLRU.c)
target_link_libraries(testProfilerLRU cache cacheAlgo traceReader profiler dataStructure traceStat utils m ${LIBS} ${CMAKE_THREAD_LIBS_INIT} -lpthread -ldl)

add_executable(testProfiler test_simulator.c)
target_link_libraries(testProfiler cache cacheAlgo traceReader profiler dataStructure traceStat utils m ${LIBS} ${CMAKE_THREAD_LIBS_INIT} -lpthread -ldl)

add_executable(testCacheAlgo test_cacheAlgo.c)
target_link_libraries(testCacheAlgo cache cache cacheAlgo traceReader profiler dataStructure traceStat utils m ${LIBS} ${CMAKE_THREAD_LIBS_INIT} -lpthread -ldl)

add_executable(testHeatmap ../test/test_heatmap.c)
target_link_libraries(testHeatmap cache cacheAlgo traceReader profiler dataStructure traceStat utils m ${LIBS} ${CMAKE_THREAD_LIBS_INIT} -lpthread -ldl)

add_test(NAME testReader COMMAND testReader WORKING_DIRECTORY .)
add_test(NAME testDistUtils COMMAND testDistUtils WORKING_DIRECTORY .)
add_test(NAME testProfilerLRU COMMAND testProfilerLRU WORKING_DIRECTORY .)
add_test(NAME testProfiler COMMAND testProfiler WORKING_DIRECTORY .)
add_test(NAME testCacheAlgo COMMAND testCacheAlgo WORKING_DIRECTORY .)
