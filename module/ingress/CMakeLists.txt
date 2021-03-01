cmake_minimum_required(VERSION 3.0)
project(hdtn-ingress)
#set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 11)
find_package(Boost 1.67.0 REQUIRED system filesystem timer thread regex filesystem program_options)

# define _WIN32_WINNT to use Windows 10 for boost
if(WIN32)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_definitions(_WIN32_WINNT=0x0A00)
endif()


add_executable(hdtn-ingress 
    src/ingress.cpp
    src/receive.cpp
    ../../common/util/src/CircularIndexBufferSingleProducerSingleConsumerConfigurable.cpp
    ../../common/util/src/SignalHandler.cpp
    ../../common/logging/src/logging.cpp
    ../../common/bpcodec/src/codec/bpv6.cpp
    ../../common/bpcodec/src/codec/bpv7.cpp
    ../../common/bpcodec/src/codec/sdnv/cgoto/decode.cpp
    ../../common/bpcodec/src/codec/sdnv/cgoto/encode.cpp
    ../../common/bpcodec/src/codec/sdnv/loop/codec.cpp
	../../common/stcp/src/StcpBundleSink.cpp
    ../../common/tcpcl/src/Tcpcl.cpp
    ../../common/tcpcl/src/TcpclBundleSink.cpp
    ../../common/tcpcl/src/sdnv.c)

if (WIN32)
	#add_compile_definitions(ZMQ_STATIC)
	set(libzmq_INCLUDE "C:/libzmq_msvc_x64/include" CACHE PATH "Folder to Libzmq headers")
	set(libzmq_LIB "C:/libzmq_msvc_x64/lib/libzmq-v141-mt-4_3_4.lib" CACHE FILEPATH "Libzmq library")
	target_link_libraries(hdtn-ingress ${libzmq_LIB})
	target_include_directories(hdtn-ingress PUBLIC ${libzmq_INCLUDE})
else()
	target_link_libraries(hdtn-ingress pthread zmq)
endif()

target_link_libraries(hdtn-ingress hdtn_reg bpcodec ${Boost_LIBRARIES})
target_include_directories(hdtn-ingress PUBLIC include ../../common/logging/include  ../../common/bpcodec/include ../../common/regsvr/lib/include ../../common/include ../../common/tcpcl/include ../../common/stcp/include ../../common/util/include ${Boost_INCLUDE_DIRS})




