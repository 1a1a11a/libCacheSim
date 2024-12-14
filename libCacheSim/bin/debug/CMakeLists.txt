
add_executable(debug main.cpp)
target_link_libraries(debug ${ALL_MODULES} ${LIBS} ${CMAKE_THREAD_LIBS_INIT} utils)

add_executable(debug_aligned aligned.c)
target_link_libraries(debug_aligned ${ALL_MODULES} ${LIBS} ${CMAKE_THREAD_LIBS_INIT} utils)

add_executable(debug_fileOp fileOp.cpp)
target_link_libraries(debug_fileOp ${ALL_MODULES} ${LIBS} ${CMAKE_THREAD_LIBS_INIT} utils)
