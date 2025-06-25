# ==============================================================================
# Utility Library
# ==============================================================================

set(utils_sources_c
    logging.c
    mymath.c
    mystr.c
    mysys.c
)

add_library(utils_lib OBJECT ${utils_sources_c})

# Set target-specific compiler flags
target_compile_options(utils_lib PRIVATE
    ${LIBCACHESIM_C_FLAGS}
)

target_include_directories(utils_lib PRIVATE
    ${GLib_INCLUDE_DIRS}
    ${libCacheSim_include_dir}
)
