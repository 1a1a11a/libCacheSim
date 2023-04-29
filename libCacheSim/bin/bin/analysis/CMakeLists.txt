

add_executable(analyze main.cpp)
target_link_libraries(analyze analysisLib dataStructureLib traceReader utils ${Boost_LIBRARIES} ${LIBS} -lpthread -ldl)
