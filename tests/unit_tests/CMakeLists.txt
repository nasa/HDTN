cmake_minimum_required(VERSION 3.0)
project(unit-tests)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 11)


set(GMOCK_LIBRARIES /usr/local/lib/libgmock.a  /usr/local/lib/libgmock_main.a)
find_package(GTest REQUIRED)
find_package(Boost 1.40.0 REQUIRED system filesystem timer thread regex filesystem program_options unit_test_framework)
include_directories(${GTEST_INCLUDE_DIRS})

add_executable(unit-tests
    src/test_main.cpp
    src/egress_ut.cpp
    src/ingress_ut.cpp
    src/store_ut.cpp
    src/cache_ut.cpp
    ../../module/egress/src/internal.cpp
    ../../module/egress/src/stcp.cpp 
    ../../module/egress/src/udp.cpp
    ../../module/ingress/src/receive.cpp
    ../../common/tcpcl/src/Tcpcl.cpp
    ../../common/tcpcl/src/TcpclBundleSink.cpp
    ../../common/tcpcl/src/TcpclBundleSource.cpp
    ../../common/tcpcl/src/sdnv.c
    ../../module/ingress/src/CircularIndexBufferSingleProducerSingleConsumerConfigurable.cpp
    ../../module/ingress/src/SignalHandler.cpp
    ../../module/storage/src/store.cpp
    ../../module/storage/src/store-worker.cpp
    ../../module/storage/src/cache/cache-basic.cpp
)

target_include_directories(unit-tests PUBLIC include 
    ../../module/egress/include
    ../../module/ingress/include
    ../../module/storage/include
        ../../common/include
	../../common/pool/include
	../../common/logging/include
	../../common/regsvr/lib/include
        ../../common/bpcodec/include
        ../../common/tcpcl/include
)

target_link_libraries(unit-tests ${GTEST_LIBRARIES} ${GMOCK_LIBRARIES}  pthread zmq hdtn_reg blosc bpcodec boost_system boost_filesystem boost_timer boost_thread boost_regex boost_filesystem boost_program_options)




add_executable(boost-unit-tests
    src/boost_test_main.cpp
    ../../common/tcpcl/test/TestTcpcl.cpp
    ../../common/tcpcl/test/TestSdnv.cpp

    ../../module/egress/src/internal.cpp
    ../../module/egress/src/stcp.cpp
    ../../module/egress/src/udp.cpp
    ../../module/ingress/src/receive.cpp
    ../../common/tcpcl/src/Tcpcl.cpp
    ../../common/tcpcl/src/TcpclBundleSink.cpp
    ../../common/tcpcl/src/TcpclBundleSource.cpp
    ../../common/tcpcl/src/sdnv.c
    ../../module/ingress/src/CircularIndexBufferSingleProducerSingleConsumerConfigurable.cpp
    ../../module/ingress/src/SignalHandler.cpp
    ../../module/storage/src/store.cpp
    ../../module/storage/src/store-worker.cpp
    ../../module/storage/src/cache/cache-basic.cpp
)

target_include_directories(boost-unit-tests PUBLIC include
    ../../module/egress/include
    ../../module/ingress/include
    ../../module/storage/include
        ../../common/include
        ../../common/pool/include
        ../../common/logging/include
        ../../common/regsvr/lib/include
        ../../common/bpcodec/include
        ../../common/tcpcl/include
)

target_link_libraries(boost-unit-tests pthread zmq hdtn_reg blosc bpcodec boost_system boost_filesystem boost_timer boost_thread boost_regex boost_filesystem boost_program_options boost_unit_test_framework)

