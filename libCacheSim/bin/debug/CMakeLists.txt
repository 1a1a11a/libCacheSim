# ==============================================================================
# Debug
# ==============================================================================

add_executable(debug main.cpp)
target_link_libraries(debug ${all_modules} ${dependency_libs})

add_executable(debug_aligned aligned.c)
target_link_libraries(debug_aligned ${all_modules} ${dependency_libs})

add_executable(debug_fileOp fileOp.cpp)
target_link_libraries(debug_fileOp ${all_modules} ${dependency_libs})

