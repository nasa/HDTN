cmake_minimum_required(VERSION 3.0)
project(bpcodec_perf)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 11)
find_package(Boost 1.67.0 REQUIRED system filesystem timer thread regex filesystem program_options)

if(NOT WIN32)
	add_executable(encode-benchmark
		encode-benchmark/encoding.cpp
	)

	target_include_directories(encode-benchmark PUBLIC ../include)
	target_link_libraries(encode-benchmark PUBLIC bpcodec)

	add_executable(bundle-benchmark
		bundle-benchmark/bundle.cpp
	)

	target_include_directories(bundle-benchmark PUBLIC ../include)
	target_link_libraries(bundle-benchmark PUBLIC bpcodec)

	add_executable(bundle-decoder
		bundle-decoder/decode.cpp
	)

	target_include_directories(bundle-decoder PUBLIC ../include)
	target_link_libraries(bundle-decoder PUBLIC bpcodec)

	add_executable(bpgen
		bpgen/bpgen.cpp
	) 

target_include_directories(bpgen PUBLIC ../include)
target_link_libraries(bpgen PUBLIC bpcodec)
endif()

add_executable(bpgen-async
    bpgen/src/BpGenAsync.cpp
    bpgen/src/BpGenAsyncMain.cpp
    ../../tcpcl/src/Tcpcl.cpp
    ../../tcpcl/src/TcpclBundleSource.cpp
    ../../stcp/src/StcpBundleSource.cpp
    ../../tcpcl/src/sdnv.c
    ../../../module/ingress/src/CircularIndexBufferSingleProducerSingleConsumerConfigurable.cpp
    ../../../module/ingress/src/SignalHandler.cpp
)
if (NOT WIN32)	
	target_link_libraries(bpgen-async PUBLIC pthread)
endif()

target_include_directories(bpgen-async PUBLIC ../include bpgen/include ../../../module/ingress/include ../../tcpcl/include ../../stcp/include ${Boost_INCLUDE_DIRS})
target_link_libraries(bpgen-async PUBLIC bpcodec ${Boost_LIBRARIES})

if (NOT WIN32)	
	add_executable(bpsink
		bpsink/bpsink.cpp
	)

	target_include_directories(bpsink PUBLIC ../include)
	target_link_libraries(bpsink PUBLIC bpcodec)
endif()

add_executable(bpsink-async
    bpsink/BpSinkAsync.cpp
    bpsink/BpSinkAsyncMain.cpp
    ../../tcpcl/src/Tcpcl.cpp
    ../../tcpcl/src/TcpclBundleSink.cpp
    ../../tcpcl/src/sdnv.c
	../../stcp/src/StcpBundleSink.cpp
    ../../../module/ingress/src/CircularIndexBufferSingleProducerSingleConsumerConfigurable.cpp
    ../../../module/ingress/src/SignalHandler.cpp
)
if (NOT WIN32)	
	target_link_libraries(bpsink-async PUBLIC pthread)
endif()

target_include_directories(bpsink-async PUBLIC ../include bpsink/include ../../../module/ingress/include ../../tcpcl/include ../../stcp/include ${Boost_INCLUDE_DIRS})
target_link_libraries(bpsink-async PUBLIC bpcodec ${Boost_LIBRARIES})

if (NOT WIN32)	
	add_executable(bpreflect
		bpreflect/bpreflect.cpp
	)


	target_include_directories(bpreflect PUBLIC ../include)
	target_link_libraries(bpreflect PUBLIC bpcodec)
endif()
