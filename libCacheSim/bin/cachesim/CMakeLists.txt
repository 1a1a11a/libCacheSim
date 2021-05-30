

add_executable(cachesim main.c cli.c)
target_link_libraries(cachesim ${ALL_MODULES} ${LIBS} ${CMAKE_THREAD_LIBS_INIT})

