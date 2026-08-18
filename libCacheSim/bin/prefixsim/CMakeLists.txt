# ==============================================================================
# prefixsim -- LLM prefix-cache simulator
# ==============================================================================

# The simulator core lives in a library so test/test_prefixsim.cpp can drive it
# in-process without going through the CLI.
add_library(prefixsim_lib STATIC
    simulator.cpp
    trace.cpp
)

target_compile_options(prefixsim_lib PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:${LIBCACHESIM_CXX_FLAGS}>
)
target_include_directories(prefixsim_lib PUBLIC
    ${libCacheSim_include_dir}
    ${GLib_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(prefixsim_lib ${PROJECT_NAME})

set_target_properties(prefixsim_lib
        PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)

add_executable(prefixsim main.cpp)

target_compile_options(prefixsim PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:${LIBCACHESIM_CXX_FLAGS}>
)
target_link_libraries(prefixsim prefixsim_lib)

set_target_properties(prefixsim
        PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)

install(TARGETS prefixsim RUNTIME DESTINATION bin)
