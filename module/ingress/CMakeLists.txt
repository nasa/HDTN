cmake_minimum_required(VERSION 3.0)
project(hdtn-ingress)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 11)

add_executable(hdtn-ingress 
    src/ingress.cpp
    src/receive.cpp
    ../../common/logging/src/logging.cpp
    ../../common/bpcodec/src/codec/bpv6.cpp
    ../../common/bpcodec/src/codec/bpv7.cpp
    ../../common/bpcodec/src/codec/sdnv/cgoto/decode.cpp
    ../../common/bpcodec/src/codec/sdnv/cgoto/encode.cpp
    ../../common/bpcodec/src/codec/sdnv/loop/codec.cpp)

target_link_libraries(hdtn-ingress pthread zmq hdtn_reg bpcodec)
target_include_directories(hdtn-ingress PUBLIC include ../../common/logging/include  ../../common/bpcodec/include ../../common/regsvr/lib/include ../../common/include)


