
add_library(traceOneHitLib oneHit.cpp)


add_executable(traceOneHit main.c cli_parser.c ../cli_reader_utils.c)
target_link_libraries(traceOneHit traceOneHitLib ${ALL_MODULES} ${LIBS} ${CMAKE_THREAD_LIBS_INIT} utils)
install(TARGETS traceOneHit RUNTIME DESTINATION bin)
