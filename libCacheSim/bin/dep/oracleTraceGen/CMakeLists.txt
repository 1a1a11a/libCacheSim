

add_executable(oracleTraceGen main.cpp)
target_link_libraries(oracleTraceGen oracleTraceGenLib dataStructureLib traceReader ${Boost_LIBRARIES})
