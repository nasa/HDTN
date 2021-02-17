cmake_minimum_required(VERSION 3.0)
project(hdtn_reg)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 11)

if (WIN32)
    set(zmq_win_path "C:/libzmq_msvc_x64/include")
endif()

add_library(hdtn_reg STATIC
    src/reg.cpp
)
target_link_libraries(hdtn_reg pthread zmq)
target_include_directories(hdtn_reg PUBLIC include ${zmq_win_path})

add_executable(hdtn_regtest
    src/reg.cpp
    src/main.cpp
)




target_link_libraries(hdtn_regtest pthread zmq)
target_include_directories(hdtn_regtest PUBLIC include)
