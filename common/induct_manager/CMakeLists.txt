cmake_minimum_required(VERSION 3.0)
project(induct_manager_lib)
set(CMAKE_CXX_STANDARD 11)



add_library(induct_manager_lib STATIC
	src/Induct.cpp
	src/TcpclInduct.cpp
	src/TcpclV4Induct.cpp
	src/StcpInduct.cpp
    src/UdpInduct.cpp
	src/LtpOverUdpInduct.cpp
	src/InductManager.cpp
)

target_link_libraries(induct_manager_lib 
	hdtn_util
	config_lib
	tcpcl_lib 
	stcp_lib 
	udp_lib 
	ltp_lib
	${Boost_LIBRARIES}
	${pthread_LIB}
)
target_include_directories(induct_manager_lib PUBLIC include ${Boost_INCLUDE_DIRS})


