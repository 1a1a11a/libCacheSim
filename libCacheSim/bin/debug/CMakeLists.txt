
add_executable(debug main.cpp)
target_link_libraries(debug ${ALL_MODULES} ${LIBS} ${CMAKE_THREAD_LIBS_INIT} analysisLib utils)
