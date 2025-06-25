# ==============================================================================
# Debug
# ==============================================================================

add_executable(debug main.cpp)
target_include_directories(debug PRIVATE ${libCacheSim_include_dir} ${GLib_INCLUDE_DIRS})
target_link_libraries(debug ${PROJECT_NAME})

add_executable(debug_aligned aligned.c)
target_include_directories(debug_aligned PRIVATE ${libCacheSim_include_dir} ${GLib_INCLUDE_DIRS})
target_link_libraries(debug_aligned ${PROJECT_NAME})

add_executable(debug_fileOp fileOp.cpp)
target_include_directories(debug_fileOp PRIVATE ${libCacheSim_include_dir} ${GLib_INCLUDE_DIRS})
target_link_libraries(debug_fileOp ${PROJECT_NAME})

