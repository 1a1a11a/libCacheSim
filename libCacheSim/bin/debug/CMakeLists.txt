# ==============================================================================
# Debug
# ==============================================================================

add_executable(debug main.cpp)
target_link_libraries(debug ${core_libs} ${dependency_libs})

add_executable(debug_aligned aligned.c)
target_link_libraries(debug_aligned ${core_libs} ${dependency_libs})

add_executable(debug_fileOp fileOp.cpp)
target_link_libraries(debug_fileOp ${core_libs} ${dependency_libs})

