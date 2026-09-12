# Helper function to create test executables with explicit compiler flags
function(add_test_executable target_name source_file)
    add_executable(${target_name} ${source_file})

    # Apply appropriate compiler flags based on source file language
    get_filename_component(file_ext ${source_file} EXT)
    if(file_ext STREQUAL ".cpp" OR file_ext STREQUAL ".cxx" OR file_ext STREQUAL ".cc")
        # C++ source file
        target_compile_options(${target_name} PRIVATE
            ${LIBCACHESIM_CXX_FLAGS}
        )
        set_target_properties(${target_name} PROPERTIES
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
        )
    else()
        # C source file
        target_compile_options(${target_name} PRIVATE
            ${LIBCACHESIM_C_FLAGS}
        )
    endif()

    target_link_libraries(${target_name} ${PROJECT_NAME})

    # Add include directories for test compilation
    target_include_directories(${target_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/libCacheSim  # Add source dir to find utils/include/mymath.h
        ${libCacheSim_include_dir}
        ${GLib_INCLUDE_DIRS}
    )
endfunction()

# Create C test executables
add_test_executable(testReader test_traceReader.c)
add_test_executable(testDistUtils test_dist.c)
add_test_executable(testProfilerLRU test_profilerLRU.c)
add_test_executable(testSimulator test_simulator.c)
add_test_executable(testEvictionAlgo test_evictionAlgo.c)
add_test_executable(testMRC test_MRC.c)
add_test_executable(testAdmissionAlgo test_admissionAlgo.c)
add_test_executable(testPrefetchAlgo test_prefetchAlgo.c)
add_test_executable(testDataStructure test_dataStructure.c)
add_test_executable(testUtils test_utils.c)

# Create C++ test executable (mrcProfiler test)
# add_test_executable(testMrcProfiler test_mrcProfiler.cpp)
# target_link_libraries(testMrcProfiler mrcProfiler_lib)
# target_link_options(testMrcProfiler PRIVATE "-Wl,--export-dynamic")



add_test(NAME testReader COMMAND testReader WORKING_DIRECTORY .)
add_test(NAME testDistUtils COMMAND testDistUtils WORKING_DIRECTORY .)
add_test(NAME testProfilerLRU COMMAND testProfilerLRU WORKING_DIRECTORY .)
add_test(NAME testSimulator COMMAND testSimulator WORKING_DIRECTORY .)
add_test(NAME testEvictionAlgo COMMAND testEvictionAlgo WORKING_DIRECTORY .)
add_test(NAME testAdmissionAlgo COMMAND testAdmissionAlgo WORKING_DIRECTORY .)
add_test(NAME testPrefetchAlgo COMMAND testPrefetchAlgo WORKING_DIRECTORY .)
add_test(NAME testDataStructure COMMAND testDataStructure WORKING_DIRECTORY .)
add_test(NAME testUtils COMMAND testUtils WORKING_DIRECTORY .)

# Exercises the command-line tools themselves: argument handling, `-e print`
# parameter reporting, and eviction parameter validation. Runs from the build
# directory so it can find bin/ and ../data/; skips itself if either is absent.
add_test(NAME testCLI
         COMMAND ${CMAKE_COMMAND} -E env bash ${CMAKE_CURRENT_SOURCE_DIR}/test_cli.sh
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
# The script prints SKIP: and exits 0 when the binaries or sample traces are
# absent. Without this, ctest reports that as a pass, so a run that tested
# nothing looks the same as a run that tested everything. The property arrived
# in CMake 3.16 and this project supports 3.12, where the run still passes
# rather than skipping -- no worse than before, just not as clear.
if(NOT CMAKE_VERSION VERSION_LESS "3.16")
  set_tests_properties(testCLI PROPERTIES SKIP_REGULAR_EXPRESSION "^SKIP:")
endif()
# add_test(NAME testMrcProfiler COMMAND testMrcProfiler WORKING_DIRECTORY .)

# if (ENABLE_GLCACHE)
#     add_executable(testGLCache test_glcache.c)
#     target_link_libraries(testGLCache ${all_modules} ${dependency_libs})
#     add_test(NAME testGLCache COMMAND testGLCache WORKING_DIRECTORY .)
# endif (ENABLE_GLCACHE)
