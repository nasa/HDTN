cmake_minimum_required(VERSION 3.0)
project(unit-tests)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 11)

set(GMOCK_LIBRARIES /usr/local/lib/libgmock.a  /usr/local/lib/libgmock_main.a)
find_package(GTest REQUIRED)
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
)

target_link_libraries(unit-tests ${GTEST_LIBRARIES} ${GMOCK_LIBRARIES}  pthread zmq hdtn_reg blosc bpcodec)

