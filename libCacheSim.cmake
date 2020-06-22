# - Config file for the libCacheSim package
# It defines the following variables
#  libCacheSim_INCLUDE_DIR - include directory
#  libCacheSim_LIBRARIES    - libraries to link against

# Compute paths
get_filename_component(libCacheSim_CMAKE_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)
set(libCacheSim_INCLUDE_DIR "/usr/local/include")

# These are IMPORTED targets created by libCacheSim-targets.cmake
set(libCacheSim_LIBRARIES "/usr/local/lib/liblibCacheSim.so")


