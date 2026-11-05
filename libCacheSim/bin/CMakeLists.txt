# ==============================================================================
# Binaries
# ==============================================================================

add_subdirectory(MRC)
add_subdirectory(cachesim)
add_subdirectory(distUtil)
add_subdirectory(traceUtils)
add_subdirectory(traceAnalyzer)
add_subdirectory(mrcProfiler)
add_subdirectory(debug)

if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/customized)
    add_subdirectory(customized/SOSP23)
endif()
