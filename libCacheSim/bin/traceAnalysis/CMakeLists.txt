
add_library(oneHit oneHit.cpp)


add_executable(traceAnalysis main.c cli.c ../cli_utils.c)
target_link_libraries(traceAnalysis oneHit ${ALL_MODULES} ${LIBS} ${CMAKE_THREAD_LIBS_INIT} utils)
install(TARGETS traceAnalysis RUNTIME DESTINATION bin)
