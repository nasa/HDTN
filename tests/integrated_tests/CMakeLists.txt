cmake_minimum_required(VERSION 3.0)
project(integrated-tests)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 11)

set(GMOCK_LIBRARIES /usr/local/lib/libgmock.a  /usr/local/lib/libgmock_main.a)
find_package(GTest REQUIRED)
find_package(Boost 1.40.0 REQUIRED system filesystem timer thread regex filesystem program_options)
include_directories(${GTEST_INCLUDE_DIRS})

#find_package(Boost 1.71.0 REQUIRED system filesystem timer thread)
find_package(Boost 1.71.0 REQUIRED filesystem)


add_executable(integrated-tests
    src/it_test_main.cpp
#    src/it_support.cpp
    src/integrated_tests.cpp
    ../../common/logging/src/logging.cpp
    ../../module/egress/src/internal.cpp
    ../../module/egress/src/stcp.cpp
    ../../module/egress/src/udp.cpp
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
)

#target_link_libraries(integrated-tests ${GTEST_LIBRARIES} ${GMOCK_LIBRARIES}  pthread zmq hdtn_reg blosc bpcodec system filesystem timer thread)
target_link_libraries(integrated-tests ${GTEST_LIBRARIES} ${GMOCK_LIBRARIES}  pthread zmq hdtn_reg blosc bpcodec boost_system boost_filesystem boost_timer boost_thread boost_regex boost_filesystem boost_program_options )
