cmake_minimum_required(VERSION 3.0)
project(hdtn_reg)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 11)
find_package(Boost 1.67.0 REQUIRED system filesystem timer thread regex filesystem program_options)

add_library(hdtn_reg STATIC
    src/reg.cpp
    ../../util/src/JsonSerializable.cpp
)
target_link_libraries(hdtn_reg pthread zmq ${Boost_LIBRARIES})
target_include_directories(hdtn_reg PUBLIC include ../../util/include ${Boost_INCLUDE_DIRS})

add_executable(hdtn_regtest
    src/reg.cpp
    src/main.cpp
    ../../util/src/JsonSerializable.cpp
)

target_link_libraries(hdtn_regtest pthread zmq ${Boost_LIBRARIES})
target_include_directories(hdtn_regtest PUBLIC include ../../util/include ${Boost_INCLUDE_DIRS})
