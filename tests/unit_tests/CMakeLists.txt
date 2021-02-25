cmake_minimum_required(VERSION 3.0)
project(unit-tests)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 11)

find_package(Boost 1.67.0 REQUIRED system filesystem timer thread regex filesystem program_options unit_test_framework)

add_executable(unit-tests
    src/test_main.cpp
    ../../common/tcpcl/test/TestTcpcl.cpp
    ../../common/tcpcl/test/TestSdnv.cpp
    ../../common/regsvr/lib/test/TestHdtnEntries.cpp
    ../../common/tcpcl/src/Tcpcl.cpp
    ../../common/tcpcl/src/sdnv.c
    ../../module/ingress/src/CircularIndexBufferSingleProducerSingleConsumerConfigurable.cpp
    src/egress_ut.cpp
    src/ingress_ut.cpp
    src/store_ut.cpp
    src/cache_ut.cpp
    ../../module/egress/src/internal.cpp
    ../../module/egress/src/stcp.cpp
    ../../module/egress/src/udp.cpp
    ../../module/ingress/src/receive.cpp
    ../../common/tcpcl/src/TcpclBundleSink.cpp
    ../../common/tcpcl/src/TcpclBundleSource.cpp
    ../../common/stcp/src/StcpBundleSink.cpp
    ../../common/stcp/src/StcpBundleSource.cpp
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
    ../../common/stcp/include
    ${Boost_INCLUDE_DIRS}
)

if (NOT WIN32)
        target_link_libraries(unit-tests pthread)
endif()

#target_link_libraries(boost-unit-tests hdtn_reg bpcodec ${Boost_LIBRARIES})
target_link_libraries(unit-tests zmq hdtn_reg blosc bpcodec ${Boost_LIBRARIES})


