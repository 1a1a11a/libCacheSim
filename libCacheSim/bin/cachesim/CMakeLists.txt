
add_executable(cachesim main.c cli.c cli_parser.c params.c sim.c ../cli_utils.c)
target_link_libraries(cachesim ${ALL_MODULES} ${LIBS} ${CMAKE_THREAD_LIBS_INIT})
install(TARGETS cachesim RUNTIME DESTINATION bin)
