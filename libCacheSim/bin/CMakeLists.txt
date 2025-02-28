

add_subdirectory(MRC)
add_subdirectory(cachesim)
# add_subdirectory(traceWriter)
add_subdirectory(distUtil)
add_subdirectory(traceUtils)
add_subdirectory(traceAnalyzer)
add_subdirectory(debug)

if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/customized)
    message(STATUS "Found customized directory, building customized")
    add_subdirectory(customized/SOSP23)
endif()

if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/priv)
    message(STATUS "Found priv directory, building priv")
    add_subdirectory(priv)
endif()

