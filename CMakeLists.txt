cmake_minimum_required(VERSION 3.0)
project(hdtn-gui)

link_directories(hdtn-gui ${CMAKE_INSTALL_PREFIX}/lib)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g -DBOOST_SPIRIT_THREADSAFE")

set(SOURCE_FILES
	HidraLink.cpp  
	HidraLinkMap.cpp  
	JsonSerializable.cpp  
	main.cpp  
	SignalHandler.cpp  
	WebsocketServer.cpp
)

set(
	HDTN_LINK_LIB
)

if(UNIX AND NOT APPLE)
	set(HDTN_LINK_LIB ${HDTN_LINK_LIB} rt)
endif()

add_executable(hdtn-gui ${SOURCE_FILES})

target_compile_definitions(hdtn-gui PUBLIC _GNU_SOURCE)
target_include_directories(hdtn-gui PUBLIC include ${CMAKE_INSTALL_PREFIX}/include)
target_link_libraries(hdtn-gui ${HDTN_LINK_LIB} pthread civetweb dl boost_thread boost_system boost_filesystem boost_regex)

