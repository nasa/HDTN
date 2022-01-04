cmake_minimum_required(VERSION 3.0)
project(hdtn_gui)
set(CMAKE_CXX_STANDARD 11)


add_library(hdtn_gui
    WebsocketServer.cpp   
)

target_compile_definitions(hdtn_gui PUBLIC _GNU_SOURCE)
target_include_directories(hdtn_gui PUBLIC . include /home/blake/hdtn/external.install/include ${Boost_INCLUDE_DIRS})
target_link_libraries(hdtn_gui ${pthread_LIB} /home/blake/hdtn/external.install/lib/libcivetweb.so  ${Boost_LIBRARIES})

