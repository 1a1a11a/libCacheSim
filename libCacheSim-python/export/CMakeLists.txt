# Helper functions are removed since we don't export source files anymore

set(EXPORT_FILE "${CMAKE_BINARY_DIR}/export_vars.cmake")
file(WRITE "${EXPORT_FILE}" "")

get_filename_component(MAIN_PROJECT_SOURCE_DIR ${CMAKE_SOURCE_DIR} ABSOLUTE)
file(WRITE ${CMAKE_BINARY_DIR}/export_vars.cmake "set(MAIN_PROJECT_SOURCE_DIR \"${MAIN_PROJECT_SOURCE_DIR}\")\n")
file(APPEND ${CMAKE_BINARY_DIR}/export_vars.cmake "set(dependency_libs \"${dependency_libs}\")\n")
file(APPEND ${CMAKE_BINARY_DIR}/export_vars.cmake "set(LIBCACHESIM_VERSION \"${LIBCACHESIM_VERSION}\")\n")

# ==============================================================================
# Export project metadata
# ==============================================================================
file(APPEND "${EXPORT_FILE}" "set(LIBCACHESIM_VERSION \"${${PROJECT_NAME}_VERSION}\")\n")

# ==============================================================================
# Export essential include directory variables
# ==============================================================================
foreach(var IN ITEMS GLib_INCLUDE_DIRS GLib_CONFIG_INCLUDE_DIR XGBOOST_INCLUDE_DIR LIGHTGBM_PATH ZSTD_INCLUDE_DIR)
    file(APPEND "${EXPORT_FILE}" "set(${var} \"${${var}}\")\n")
endforeach()

# ==============================================================================
# Export dependency library variables
# ==============================================================================
file(APPEND "${EXPORT_FILE}" "set(dependency_libs \"${dependency_libs}\")\n")

# ==============================================================================
# Export essential build option variables
# ==============================================================================
file(APPEND "${EXPORT_FILE}" "set(LOG_LEVEL_LOWER \"${LOG_LEVEL_LOWER}\")\n")

message(STATUS "Exported essential variables to ${EXPORT_FILE}")
