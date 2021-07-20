cmake_minimum_required(VERSION 3.0)
project(outduct_manager_lib)
set(CMAKE_CXX_STANDARD 11)



add_library(outduct_manager_lib STATIC
	src/Outduct.cpp
	src/TcpclOutduct.cpp
	src/StcpOutduct.cpp
    src/UdpOutduct.cpp
	src/LtpOverUdpOutduct.cpp
	src/OutductManager.cpp
)

target_link_libraries(outduct_manager_lib 
	hdtn_util
	config_lib
	tcpcl_lib 
	stcp_lib 
	udp_lib 
	ltp_lib
	${Boost_LIBRARIES}
	${pthread_LIB}
)
target_include_directories(outduct_manager_lib PUBLIC include ${Boost_INCLUDE_DIRS})


