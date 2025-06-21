# ==============================================================================
# Utility Library
# ==============================================================================

set(utils_sources_c
    logging.c
    mymath.c
    mystr.c
    mysys.c
)

add_library(utils_lib ${utils_sources_c})

# Set target-specific compiler flags
target_compile_options(utils_lib PRIVATE
    ${LIBCACHESIM_C_FLAGS}
    ${tcmalloc_flags}
)

target_include_directories(utils_lib PRIVATE 
    ${GLib_INCLUDE_DIRS}
    ${libCacheSim_include_dir}
    ${libCacheSim_binary_include_dir}
)

set(utils_sources ${utils_sources_c} PARENT_SCOPE)
