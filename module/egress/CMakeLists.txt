cmake_minimum_required(VERSION 3.0)
project(hdtn-egress)
#set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 11)


if (NOT WIN32)	
	add_executable(hdtn-egress 
		src/egress.cpp
		src/internal.cpp
		src/udp.cpp
		src/stcp.cpp 
		)

	target_link_libraries(hdtn-egress
        pthread
	    zmq
	    hdtn_reg 
        log_lib
		bpcodec 
		${Boost_LIBRARIES}
)

target_include_directories(hdtn-egress PUBLIC 
	include 
	../../common/pool/include  
	../../common/bpcodec/include 
	../../common/regsvr/lib/include 
	../../common/include)
endif()

add_library(egress_async_lib STATIC
	src/EgressAsync.cpp
	src/EgressAsyncRunner.cpp
    src/EgressAsyncTcpcl.cpp
	src/EgressAsyncStcp.cpp
)

target_link_libraries(egress_async_lib
    hdtn_reg
    hdtn_util
    log_lib
    tcpcl_lib
    stcp_lib
	udp_lib
	${libzmq_LIB}
    ${pthread_LIB}
    ${blosc_LIB}
    bpcodec
    ${Boost_LIBRARIES}
)
target_include_directories(egress_async_lib PUBLIC
	include
	../../common/pool/include
	../../common/include
	${Boost_INCLUDE_DIRS} 
	${libzmq_INCLUDE}
)

add_executable(hdtn-egress-async
	src/EgressAsyncMain.cpp
)

target_link_libraries(hdtn-egress-async hdtn_reg hdtn_util tcpcl_lib stcp_lib udp_lib egress_async_lib ${libzmq_LIB} ${pthread_LIB} ${blosc_LIB} bpcodec ${Boost_LIBRARIES})
target_include_directories(hdtn-egress-async PUBLIC 
	../../common/pool/include
	../../common/include
	${Boost_INCLUDE_DIRS} 
	${libzmq_INCLUDE}
)

