


add_executable(testReader ../test/test_traceReader.c)
target_link_libraries(testReader cache evictionAlgo traceReader profiler dataStructure utils ${LIBS})

add_executable(testDistUtils test_dist.c)
target_link_libraries(testDistUtils cache evictionAlgo traceReader profiler dataStructure utils ${LIBS})

add_executable(testProfilerLRU ../test/test_profilerLRU.c)
target_link_libraries(testProfilerLRU cache evictionAlgo traceReader profiler dataStructure utils ${LIBS})

add_executable(testSimulator test_simulator.c)
target_link_libraries(testSimulator cache evictionAlgo traceReader profiler dataStructure utils ${LIBS}
        ${CMAKE_THREAD_LIBS_INIT} -ldl)

add_executable(testEvictionAlgo test_evictionAlgo.c)
target_link_libraries(testEvictionAlgo cache evictionAlgo traceReader profiler dataStructure utils ${LIBS})

add_executable(testHeatmap ../test/test_heatmap.c)
target_link_libraries(testHeatmap cache evictionAlgo traceReader profiler dataStructure utils ${LIBS})

add_executable(debug ../test/debug.c)
target_link_libraries(debug cache evictionAlgo traceReader profiler dataStructure utils ${LIBS})

add_test(NAME testReader COMMAND testReader WORKING_DIRECTORY .)
add_test(NAME testDistUtils COMMAND testDistUtils WORKING_DIRECTORY .)
add_test(NAME testProfilerLRU COMMAND testProfilerLRU WORKING_DIRECTORY .)
add_test(NAME testSimulator COMMAND testSimulator WORKING_DIRECTORY .)
add_test(NAME testEvictionAlgo COMMAND testEvictionAlgo WORKING_DIRECTORY .)
