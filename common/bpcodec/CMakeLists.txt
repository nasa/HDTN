cmake_minimum_required(VERSION 3.0)
project(bpcodec)
#set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 11)



add_library(bpcodec STATIC
        src/codec/bpv7.cpp
        src/codec/bpv6.cpp
        src/codec/bpv6-ext-block.cpp
		src/codec/Bpv6AdministrativeRecords.cpp
        src/codec/cbor/cgoto/codec.cpp
        src/codec/cbor/standard/codec.cpp
        src/codec/cbor/memo/codec.cpp
        src/codec/crc.cpp
)


#add_compile_definitions(BPV7_CBOR_CGOTO)
add_compile_definitions(BPV7_CBOR_STD)
#add_compile_definitions(BPV7_CBOR_MEMOIZE)

target_link_libraries(bpcodec hdtn_util)
target_include_directories(bpcodec PUBLIC include)

add_subdirectory(apps)

