
add_library(traceOneHitLib oneHit/oneHit.cpp)
add_executable(traceOneHit oneHit/main.c oneHit/oneHit.cpp oneHit/cli_parser.c ../../cli_reader_utils.c)
target_link_libraries(traceOneHit traceOneHitLib ${ALL_MODULES} ${LIBS} ${CMAKE_THREAD_LIBS_INIT} utils)


add_executable(flash flash/flash.cpp ../../cachesim/cli_parser.c ../../cachesim/sim.c ../../cli_reader_utils.c)
target_link_libraries(flash ${ALL_MODULES} ${LIBS} ${CMAKE_THREAD_LIBS_INIT} utils)
