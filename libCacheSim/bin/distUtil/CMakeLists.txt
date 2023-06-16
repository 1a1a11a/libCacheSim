
add_executable(distUtil main.c cli.c ../cli_reader_utils.c)
target_link_libraries(distUtil ${ALL_MODULES} ${LIBS} ${CMAKE_THREAD_LIBS_INIT} utils)
install(TARGETS cachesim RUNTIME DESTINATION bin)
