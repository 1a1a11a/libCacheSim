

add_library(cliReaderLib ../cli_reader_utils.c)


add_executable(tracePrint tracePrintMain.cpp cli_parser.cpp)
target_link_libraries(tracePrint cliReaderLib ${ALL_MODULES} ${LIBS} ${CMAKE_THREAD_LIBS_INIT})
set_target_properties(tracePrint
        PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        )


add_executable(traceConv traceConvMain.cpp traceConvOracleGeneral.cpp traceConvLCS.cpp cli_parser.cpp utils.cpp)
target_link_libraries(traceConv cliReaderLib ${ALL_MODULES} ${LIBS} ${CMAKE_THREAD_LIBS_INIT})
set_target_properties(traceConv
        PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        )

add_executable(traceFilter traceFilterMain.cpp cli_parser.cpp)
target_link_libraries(traceFilter cliReaderLib ${ALL_MODULES} ${LIBS} ${CMAKE_THREAD_LIBS_INIT})
set_target_properties(traceFilter
        PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        )

install(TARGETS traceConv RUNTIME DESTINATION bin)
install(TARGETS tracePrint RUNTIME DESTINATION bin)
install(TARGETS traceFilter RUNTIME DESTINATION bin)

