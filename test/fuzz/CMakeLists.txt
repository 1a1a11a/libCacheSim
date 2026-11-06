# libFuzzer targets, built with -DENABLE_FUZZING=ON. The top-level
# CMakeLists.txt adds the coverage and sanitizer flags to every target, so
# the library code the targets call is instrumented as well.

add_executable(fuzz_traceReader fuzz_traceReader.c)
target_compile_options(fuzz_traceReader PRIVATE ${LIBCACHESIM_C_FLAGS})
target_include_directories(fuzz_traceReader PRIVATE ${GLib_INCLUDE_DIRS})
target_link_libraries(fuzz_traceReader ${PROJECT_NAME})
# -fsanitize=fuzzer links libFuzzer's main(); the library objects only get
# fuzzer-no-link, which instruments without adding a main
set_target_properties(fuzz_traceReader PROPERTIES LINK_FLAGS "-fsanitize=fuzzer")
