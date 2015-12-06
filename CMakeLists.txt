cmake_minimum_required(VERSION 2.8.12)
project(stfutures CXX)

set(CMAKE_THREAD_LIBS_INIT -lpthread)
set(warnings "-Wall -Wextra -Werror -Wno-parentheses -Wno-deprecated-declarations")
set(opt "-O3")
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(opt "-O0 -g -DSPDLOG_TRACE_ON")
endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${warnings} ${opt}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")

add_executable(stfutures 
    main.cpp
    )
target_link_libraries(stfutures ${CMAKE_THREAD_LIBS_INIT})
