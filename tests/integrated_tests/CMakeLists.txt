cmake_minimum_required(VERSION 3.0)
project(integrated-tests)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 11)

find_package(Boost 1.67.0 REQUIRED system filesystem timer thread regex filesystem program_options unit_test_framework)

if(NOT WIN32)
	add_executable(integrated-tests
		src/it_test_main.cpp
		src/integrated_tests.cpp
		../../common/logging/src/logging.cpp
		../../common/bpcodec/apps/bpsink/BpSinkAsync.cpp
		../../common/tcpcl/src/Tcpcl.cpp
		../../common/tcpcl/src/TcpclBundleSink.cpp
		../../common/tcpcl/src/TcpclBundleSource.cpp

		../../common/stcp/src/StcpBundleSink.cpp
		../../common/stcp/src/StcpBundleSource.cpp

		../../common/tcpcl/src/sdnv.c
		../../module/egress/src/internal.cpp
		../../module/egress/src/stcp.cpp
		../../module/egress/src/udp.cpp
		../../module/egress/src/EgressAsync.cpp
		../../module/egress/src/EgressAsyncTcpcl.cpp
		../../module/egress/src/EgressAsyncStcp.cpp
		../../module/ingress/src/receive.cpp
		../../module/ingress/src/CircularIndexBufferSingleProducerSingleConsumerConfigurable.cpp
		../../module/ingress/src/SignalHandler.cpp
		../../module/storage/src/store.cpp
		../../module/storage/src/store-worker.cpp
		../../module/storage/src/cache/cache-basic.cpp
	)

	target_include_directories(integrated-tests PUBLIC include
		../../module/egress/include
		../../module/ingress/include
		../../module/storage/include
		../../common/include
		../../common/pool/include
		../../common/logging/include
		../../common/regsvr/lib/include
		../../common/bpcodec/include
		../../common/bpcodec/apps/bpsink/include
		../../common/tcpcl/include
		../../common/stcp/include
		${Boost_INCLUDE_DIRS}
	)

	target_link_libraries(integrated-tests pthread zmq hdtn_reg blosc bpcodec ${Boost_LIBRARIES} )

endif()


