
add_executable(traceAnalyzer main.cpp ../cli_reader_utils.c)
target_link_libraries(traceAnalyzer ${ALL_MODULES} ${LIBS} ${CMAKE_THREAD_LIBS_INIT} utils)
install(TARGETS traceAnalyzer RUNTIME DESTINATION bin)
