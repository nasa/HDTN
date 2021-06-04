cmake_minimum_required(VERSION 3.0)
project(hdtn-ingress)
set(CMAKE_CXX_STANDARD 11)

add_library(ingress_async_lib STATIC
	src/receive.cpp
	src/IngressAsyncRunner.cpp
)

target_link_libraries(ingress_async_lib 
	hdtn_reg 
	hdtn_util 
    log_lib
	tcpcl_lib 
	stcp_lib 
	udp_lib 
	ltp_lib
	${libzmq_LIB} 
	${pthread_LIB}
	bpcodec 
	${Boost_LIBRARIES}
	)
target_include_directories(ingress_async_lib PUBLIC
	include
	../../common/pool/include
	../../common/include
	${Boost_INCLUDE_DIRS} 
	${libzmq_INCLUDE}
)

add_executable(hdtn-ingress 
    src/ingress.cpp
)

target_link_libraries(hdtn-ingress
	hdtn_reg
	bpcodec
	hdtn_util
        log_lib
	tcpcl_lib
	stcp_lib
	udp_lib
	ingress_async_lib
	${libzmq_LIB}
	${pthread_LIB}
	${Boost_LIBRARIES}
)
target_include_directories(hdtn-ingress PUBLIC
	${Boost_INCLUDE_DIRS}
	${libzmq_INCLUDE}
)

