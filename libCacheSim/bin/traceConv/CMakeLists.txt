

add_library(cli_utils ../cli_utils.c)


add_executable(tracePrint tracePrintMain.cpp cli.cpp)
target_link_libraries(tracePrint cli_utils ${ALL_MODULES} ${LIBS} ${CMAKE_THREAD_LIBS_INIT})
set_target_properties(tracePrint
        PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        )


add_executable(traceConv traceConvMain.cpp traceConv.cpp cli.cpp)
target_link_libraries(traceConv cli_utils ${ALL_MODULES} ${LIBS} ${CMAKE_THREAD_LIBS_INIT})
set_target_properties(traceConv
        PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        )
install(TARGETS traceConv RUNTIME DESTINATION bin)
install(TARGETS tracePrint RUNTIME DESTINATION bin)


