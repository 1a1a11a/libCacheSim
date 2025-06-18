# ==============================================================================
# Utility Library
# ==============================================================================

set(UTILS_SOURCES
    logging.c
    mymath.c
    mystr.c
    mysys.c
)

add_library(utils ${UTILS_SOURCES})
target_include_directories(utils PUBLIC 
    ${GLib_INCLUDE_DIRS}
    ${libCacheSim_include_dir}
    ${libCacheSim_binary_include_dir}
)




