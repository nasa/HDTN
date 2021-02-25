cmake_minimum_required(VERSION 3.0)
project(hdtn-egress)
#set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 11)
find_package(Boost 1.67.0 REQUIRED system filesystem timer thread regex filesystem program_options)

if (NOT WIN32)	
	add_executable(hdtn-egress 
		src/egress.cpp
		src/internal.cpp
		src/udp.cpp
		src/stcp.cpp
		../../common/logging/src/logging.cpp
		../../common/bpcodec/src/codec/bpv6.cpp
		../../common/bpcodec/src/codec/bpv7.cpp
		../../common/bpcodec/src/codec/sdnv/cgoto/decode.cpp
		../../common/bpcodec/src/codec/sdnv/cgoto/encode.cpp
		../../common/bpcodec/src/codec/sdnv/loop/codec.cpp)

	target_link_libraries(hdtn-egress pthread zmq hdtn_reg bpcodec)
	target_include_directories(hdtn-egress PUBLIC include ../../common/logging/include ../../common/pool/include  ../../common/bpcodec/include ../../common/regsvr/lib/include ../../common/include)
endif()

add_executable(hdtn-egress-async
    src/EgressAsync.cpp
    src/EgressAsyncMain.cpp
    src/EgressAsyncTcpcl.cpp
	src/EgressAsyncStcp.cpp
    ../ingress/src/SignalHandler.cpp
    ../../common/logging/src/logging.cpp
    ../../common/bpcodec/src/codec/bpv6.cpp
    ../../common/bpcodec/src/codec/bpv7.cpp
    ../../common/bpcodec/src/codec/sdnv/cgoto/decode.cpp
    ../../common/bpcodec/src/codec/sdnv/cgoto/encode.cpp
    ../../common/bpcodec/src/codec/sdnv/loop/codec.cpp
	../../common/stcp/src/StcpBundleSource.cpp
    ../../common/tcpcl/src/Tcpcl.cpp
    ../../common/tcpcl/src/TcpclBundleSource.cpp
    ../../common/tcpcl/src/sdnv.c)

if (WIN32)
	#add_compile_definitions(ZMQ_STATIC)
	set(libzmq_INCLUDE "C:/libzmq_msvc_x64/include" CACHE PATH "Folder to Libzmq headers")
	set(libzmq_LIB "C:/libzmq_msvc_x64/lib/libzmq-v141-mt-4_3_4.lib" CACHE FILEPATH "Libzmq library")
	target_link_libraries(hdtn-egress-async ${libzmq_LIB})
	target_include_directories(hdtn-egress-async PUBLIC ${libzmq_INCLUDE})
else()
	target_link_libraries(hdtn-egress-async pthread zmq)
endif()
target_link_libraries(hdtn-egress-async hdtn_reg bpcodec ${Boost_LIBRARIES})
target_include_directories(hdtn-egress-async PUBLIC include ../../common/logging/include ../../common/pool/include  ../../common/bpcodec/include ../../common/regsvr/lib/include ../../common/include ../ingress/include ../../common/tcpcl/include ../../common/stcp/include ${Boost_INCLUDE_DIRS})


