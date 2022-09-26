


add_subdirectory(cachesim)
add_subdirectory(benchmark)
add_subdirectory(traceWriter)

if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/priv)
    message(STATUS "Found priv directory, building priv")
    add_subdirectory(priv)
endif()
