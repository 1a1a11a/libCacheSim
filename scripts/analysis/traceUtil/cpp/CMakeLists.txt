cmake_minimum_required(VERSION 3.2)
project(traceUtil)
set(DESCRIPTION "a simple trace utility")

set(${PROJECT_NAME}_VERSION_MAJOR 0)
set(${PROJECT_NAME}_VERSION_MINOR 1)
set(${PROJECT_NAME}_VERSION_PATCH 0)
set(${PROJECT_NAME}_RELEASE_VERSION ${${PROJECT_NAME}_VERSION_MAJOR}.${${PROJECT_NAME}_VERSION_MINOR})
set(${PROJECT_NAME}_VERSION ${${PROJECT_NAME}_RELEASE_VERSION}.${${PROJECT_NAME}_VERSION_PATCH})


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


set(CFLAGS "$ENV{CFLAGS} "
        "-Wall -Wshadow -Winline "
        "-Wno-unused "
        "-Wredundant-decls "
    )

string(REPLACE "" "" LOCAL_CFLAGS ${CFLAGS})
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} $ENV{CFLAGS} ${LOCAL_CFLAGS}")


if (CMAKE_BUILD_TYPE MATCHES Debug)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g -ggdb3 -DLOGLEVEL=5 ")
    message(STATUS "Debug build")
else ()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
    message(STATUS "Release build")
endif ()


add_executable(conv traceConv.cpp)

add_executable(split traceSplit.cpp)
