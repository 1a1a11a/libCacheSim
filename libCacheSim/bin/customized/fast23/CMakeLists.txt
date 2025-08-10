# ==============================================================================
# Fast23
# ==============================================================================

set(fast23_sources_cpp
    main.cpp
    compareGrouping.cpp
    objectInfo.cpp
    compareGroups.cpp
)
add_executable(fast23 ${fast23_sources_cpp})
target_include_directories(fast23 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${libCacheSim_include_dir}
)
target_link_libraries(fast23 dataStructure_lib traceReader_lib ${Boost_LIBRARIES})
