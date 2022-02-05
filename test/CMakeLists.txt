
# message(STATUS "ALL_MODULES = ${ALL_MODULES}")
# set(ALL_MODULES cachelib admission eviction traceReader profiler dataStructure utils)
set(coreLib ${ALL_MODULES} ${LIBS} ${CMAKE_THREAD_LIBS_INIT} -ldl)

add_executable(testReader test_traceReader.c)
target_link_libraries(testReader ${coreLib})

add_executable(testDistUtils test_dist.c)
target_link_libraries(testDistUtils ${coreLib})

add_executable(testProfilerLRU test_profilerLRU.c)
target_link_libraries(testProfilerLRU ${coreLib})

add_executable(testSimulator test_simulator.c)
target_link_libraries(testSimulator ${coreLib})

add_executable(testEvictionAlgo test_evictionAlgo.c)
target_link_libraries(testEvictionAlgo ${coreLib})


add_test(NAME testReader COMMAND testReader WORKING_DIRECTORY .)
add_test(NAME testDistUtils COMMAND testDistUtils WORKING_DIRECTORY .)
add_test(NAME testProfilerLRU COMMAND testProfilerLRU WORKING_DIRECTORY .)
add_test(NAME testSimulator COMMAND testSimulator WORKING_DIRECTORY .)
add_test(NAME testEvictionAlgo COMMAND testEvictionAlgo WORKING_DIRECTORY .)

if (ENABLE_L2CACHE)
    add_executable(testL2Cache test_l2cache.c)
    target_link_libraries(testL2Cache ${coreLib})
    add_test(NAME testL2Cache COMMAND testL2Cache WORKING_DIRECTORY .)
endif (ENABLE_L2CACHE)





