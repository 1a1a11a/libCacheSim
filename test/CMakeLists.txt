
set(coreLib ${ALL_MODULES} ${LIBS} ${CMAKE_THREAD_LIBS_INIT})

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

add_executable(testMRC test_MRC.c)
target_link_libraries(testMRC ${coreLib})

add_executable(testAdmissionAlgo test_admissionAlgo.c)
target_link_libraries(testAdmissionAlgo ${coreLib})


add_executable(testPrefetchAlgo test_prefetchAlgo.c)
target_link_libraries(testPrefetchAlgo ${coreLib})

add_executable(testDataStructure test_dataStructure.c)
target_link_libraries(testDataStructure ${coreLib})

add_executable(testUtils test_utils.c)
target_link_libraries(testUtils ${coreLib})

add_test(NAME testReader COMMAND testReader WORKING_DIRECTORY .)
add_test(NAME testDistUtils COMMAND testDistUtils WORKING_DIRECTORY .)
add_test(NAME testProfilerLRU COMMAND testProfilerLRU WORKING_DIRECTORY .)
add_test(NAME testSimulator COMMAND testSimulator WORKING_DIRECTORY .)
add_test(NAME testEvictionAlgo COMMAND testEvictionAlgo WORKING_DIRECTORY .)
add_test(NAME testAdmissionAlgo COMMAND testAdmissionAlgo WORKING_DIRECTORY .)
add_test(NAME testPrefetchAlgo COMMAND testPrefetchAlgo WORKING_DIRECTORY .)
add_test(NAME testDataStructure COMMAND testDataStructure WORKING_DIRECTORY .)
add_test(NAME testUtils COMMAND testUtils WORKING_DIRECTORY .)

# if (ENABLE_GLCACHE)
#     add_executable(testGLCache test_glcache.c)
#     target_link_libraries(testGLCache ${coreLib})
#     add_test(NAME testGLCache COMMAND testGLCache WORKING_DIRECTORY .)
# endif (ENABLE_GLCACHE)

if (ENABLE_3L_CACHE)
    add_executable(test3LCache test_3lcache.c)
    target_link_libraries(test3LCache ${coreLib})
    add_test(NAME test3LCache COMMAND test3LCache WORKING_DIRECTORY .)
endif (ENABLE_3L_CACHE)