cmake_minimum_required(VERSION 2.8.12)
project(stfutures CXX)

set(warnings "-Wall -Wextra -Werror -Wno-parentheses -Wno-deprecated-declarations")
set(opt "-O3")
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(opt "-O0 -g -DSPDLOG_TRACE_ON")
endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${warnings} ${opt}")

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
if(COMPILER_SUPPORTS_CXX11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
else()
    message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support.
                    Please use a different C++ compiler.")
endif() 

add_executable(stfutures 
    main.cpp
    )
