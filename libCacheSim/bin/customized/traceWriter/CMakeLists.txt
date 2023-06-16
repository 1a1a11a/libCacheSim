

add_executable(traceWriter main.c)
target_link_libraries(traceWriter ${ALL_MODULES} ${LIBS} ${CMAKE_THREAD_LIBS_INIT})

