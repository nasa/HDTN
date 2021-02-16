cmake_minimum_required(VERSION 3.0)
project(hdtn-egress)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 11)
find_package(Boost 1.67.0 REQUIRED system filesystem timer thread regex filesystem program_options)

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


add_executable(hdtn-egress-async
    src/EgressAsync.cpp
    src/EgressAsyncMain.cpp
    src/EgressAsyncTcpcl.cpp
    ../ingress/src/SignalHandler.cpp
    ../../common/logging/src/logging.cpp
    ../../common/bpcodec/src/codec/bpv6.cpp
    ../../common/bpcodec/src/codec/bpv7.cpp
    ../../common/bpcodec/src/codec/sdnv/cgoto/decode.cpp
    ../../common/bpcodec/src/codec/sdnv/cgoto/encode.cpp
    ../../common/bpcodec/src/codec/sdnv/loop/codec.cpp
    ../../common/tcpcl/src/Tcpcl.cpp
    ../../common/tcpcl/src/TcpclBundleSource.cpp
    ../../common/tcpcl/src/sdnv.c)

target_link_libraries(hdtn-egress-async pthread zmq hdtn_reg bpcodec ${Boost_LIBRARIES})
target_include_directories(hdtn-egress-async PUBLIC include ../../common/logging/include ../../common/pool/include  ../../common/bpcodec/include ../../common/regsvr/lib/include ../../common/include ../ingress/include ../../common/tcpcl/include ${Boost_INCLUDE_DIRS})


