


add_subdirectory(cachesim)
# add_subdirectory(traceWriter)
add_subdirectory(traceConv)
add_subdirectory(distUtil)

if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/priv)
    message(STATUS "Found priv directory, building priv")
    add_subdirectory(priv)
endif()
