cmake_minimum_required(VERSION 3.0)
project(hdtn-gui)
set(CMAKE_CXX_STANDARD 11)

link_directories(hdtn-gui ${CMAKE_INSTALL_PREFIX}/lib)


add_executable(hdtn-gui 
    WebsocketServer.cpp
    main.cpp
)


target_compile_definitions(hdtn-gui PUBLIC _GNU_SOURCE)
target_include_directories(hdtn-gui PUBLIC include ${CMAKE_INSTALL_PREFIX}/include)
target_link_libraries(hdtn-gui pthread civetweb dl boost_thread boost_system boost_filesystem boost_regex)

