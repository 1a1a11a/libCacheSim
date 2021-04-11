

add_executable(cachesim cachesim.c)
target_link_libraries(cachesim cache evictionAlgo traceReader profiler dataStructure traceStat utils)



