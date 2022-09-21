

add_executable(lesscache main.c)
target_link_libraries(lesscache ${ALL_MODULES} ${LIBS} ${CMAKE_THREAD_LIBS_INIT})
