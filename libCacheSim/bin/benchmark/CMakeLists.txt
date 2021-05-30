

add_executable(thrptBenchmark cacheAlgoPerf.c main.c)
target_link_libraries(thrptBenchmark ${ALL_MODULES} ${LIBS} ${CMAKE_THREAD_LIBS_INIT})


