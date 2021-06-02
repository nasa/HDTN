cmake_minimum_required(VERSION 3.0)
project(integrated-tests)
set(CMAKE_CXX_STANDARD 11)

SET(STORAGE_LIBRARY_NAME storage_lib)
add_executable(integrated-tests
	src/it_test_main.cpp
	src/integrated_tests.cpp
	../../common/logger/src/Logger.cpp
        ../../common/bpcodec/apps/bpgen/src/BpGenAsync.cpp
        ../../common/bpcodec/apps/bpgen/src/BpGenAsyncRunner.cpp
        ../../common/bpcodec/apps/bpsink/BpSinkAsyncRunner.cpp
        ../../common/bpcodec/apps/bpsink/BpSinkAsync.cpp
        ../../module/storage/src/store.cpp
        ../../module/storage/src/StorageRunner.cpp
        ../../module/storage/src/test/ReleaseSender.cpp
)

target_include_directories(integrated-tests PUBLIC include
    ../../module/storage/src/test
    ../../module/storage/include
    ../../common/include
    ../../common/pool/include
    ../../common/logger/include
    ../../common/regsvr/lib/include
    ../../common/bpcodec/include
    ../../common/bpcodec/apps/bpgen/include
    ../../common/bpcodec/apps/bpsink/include
    ${Boost_INCLUDE_DIRS}
    ${libzmq_INCLUDE}
)

target_link_libraries(integrated-tests 
	${libzmq_LIB}
	${pthread_LIB}
	${STORAGE_LIBRARY_NAME}
	hdtn_reg
	hdtn_util
	tcpcl_lib
	stcp_lib
	udp_lib
	egress_async_lib
	ingress_async_lib
	bpcodec
	${Boost_LIBRARIES}
)
