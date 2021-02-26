cmake_minimum_required(VERSION 3.0)
project(bpcodec)
#set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 11)
find_package(Boost 1.67.0 REQUIRED system filesystem timer thread regex filesystem program_options)

add_library(bpcodec 
        src/codec/bpv7.cpp
        src/codec/bpv6.cpp
        src/codec/bpv6-ext-block.cpp
        src/codec/sdnv/cgoto/decode.cpp
        src/codec/sdnv/cgoto/encode.cpp
        src/codec/sdnv/loop/codec.cpp
        src/codec/cbor/cgoto/codec.cpp
        src/codec/cbor/standard/codec.cpp
        src/codec/cbor/memo/codec.cpp
        src/codec/crc.cpp
)

# This library ships with a few different options for encoding / decoding values.
# Once can give them a try to see what the effects on performance look like.
#add_compile_definitions(BPV6_SDNV_CGOTO)
add_compile_definitions(BPV6_SDNV_LOOP)

#add_compile_definitions(BPV7_CBOR_CGOTO)
add_compile_definitions(BPV7_CBOR_STD)
#add_compile_definitions(BPV7_CBOR_MEMOIZE)

target_include_directories(bpcodec PUBLIC /opt/local/include include ${Boost_INCLUDE_DIRS})

add_subdirectory(apps)

