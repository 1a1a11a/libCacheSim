

add_executable(fast23 main.cpp compareGrouping.cpp objectInfo.cpp compareGroups.cpp)
target_link_libraries(fast23 analysisLib dataStructureLib traceReader ${Boost_LIBRARIES})
