# Version parsing utilities for libCacheSim
# Based on xgboost version handling

function(parse_version_from_file VERSION_FILE VERSION_MAJOR_VAR VERSION_MINOR_VAR VERSION_PATCH_VAR)
    if(EXISTS "${VERSION_FILE}")
        file(READ "${VERSION_FILE}" VERSION_STRING)
        string(STRIP "${VERSION_STRING}" VERSION_STRING)
        
        # Parse MAJOR.MINOR.PATCH format
        string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)" VERSION_MATCH "${VERSION_STRING}")
        if(VERSION_MATCH)
            set(${VERSION_MAJOR_VAR} ${CMAKE_MATCH_1} PARENT_SCOPE)
            set(${VERSION_MINOR_VAR} ${CMAKE_MATCH_2} PARENT_SCOPE)
            set(${VERSION_PATCH_VAR} ${CMAKE_MATCH_3} PARENT_SCOPE)
            message(STATUS "Version from ${VERSION_FILE}: ${VERSION_STRING}")
        else()
            message(FATAL_ERROR "Invalid version format in ${VERSION_FILE}: ${VERSION_STRING}. Expected format: MAJOR.MINOR.PATCH")
        endif()
    else()
        message(FATAL_ERROR "Version file not found: ${VERSION_FILE}")
    endif()
endfunction()

function(setup_project_version PROJECT_NAME VERSION_FILE)
    if(EXISTS "${VERSION_FILE}")
        parse_version_from_file("${VERSION_FILE}" VERSION_MAJOR VERSION_MINOR VERSION_PATCH)
        set(${PROJECT_NAME}_VERSION_MAJOR ${VERSION_MAJOR} PARENT_SCOPE)
        set(${PROJECT_NAME}_VERSION_MINOR ${VERSION_MINOR} PARENT_SCOPE)
        set(${PROJECT_NAME}_VERSION_PATCH ${VERSION_PATCH} PARENT_SCOPE)
        set(${PROJECT_NAME}_RELEASE_VERSION ${VERSION_MAJOR}.${VERSION_MINOR} PARENT_SCOPE)
        set(${PROJECT_NAME}_VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH} PARENT_SCOPE)
    else()
        message(STATUS "${VERSION_FILE} not found, using default version")
        set(${PROJECT_NAME}_VERSION_MAJOR 0 PARENT_SCOPE)
        set(${PROJECT_NAME}_VERSION_MINOR 1 PARENT_SCOPE)
        set(${PROJECT_NAME}_VERSION_PATCH 0 PARENT_SCOPE)
        set(${PROJECT_NAME}_RELEASE_VERSION 0.1 PARENT_SCOPE)
        set(${PROJECT_NAME}_VERSION 0.1.0 PARENT_SCOPE)
    endif()
endfunction()

# Legacy function for xgboost compatibility
function(write_version)
    message(STATUS "xgboost VERSION: ${xgboost_VERSION}")
    configure_file(
        ${xgboost_SOURCE_DIR}/include/config.h.in
        ${xgboost_SOURCE_DIR}/include/config.h @ONLY)
#   configure_file(
#     ${xgboost_SOURCE_DIR}/cmake/Python_version.in
#     ${xgboost_SOURCE_DIR}/python-package/xgboost/VERSION @ONLY)
endfunction(write_version)
