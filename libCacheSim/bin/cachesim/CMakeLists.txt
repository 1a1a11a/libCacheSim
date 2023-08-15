
add_executable(cachesim main.c cli_parser.c sim.c ../cli_reader_utils.c)
target_link_libraries(cachesim ${ALL_MODULES} ${LIBS} ${CMAKE_THREAD_LIBS_INIT} utils)
install(TARGETS cachesim RUNTIME DESTINATION bin)

# add_executable(flash flash.cpp cli_parser.c sim.c ../cli_reader_utils.c)
# target_link_libraries(flash ${ALL_MODULES} ${LIBS} ${CMAKE_THREAD_LIBS_INIT} utils)
