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
        ../../module/scheduler/src/scheduler.cpp
)

target_include_directories(integrated-tests PUBLIC include
    ../../module/storage/src/test
    ../../module/storage/include
    ../../module/scheduler/include
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
	ltp_lib
	egress_async_lib
	ingress_async_lib
	induct_manager_lib
	outduct_manager_lib
	bpcodec
	bp_app_patterns_lib
	${Boost_LIBRARIES}
)

add_executable(cgr-test
    src/cgr_tests.cpp
    ../../module/scheduler/src/cgrServer.cpp
)

target_include_directories(cgr-test PUBLIC include
    ../../module/scheduler/include
        lib/include
    include
        ../../common/bpcodec/include
        ../../common/regsvr/lib/include
        ../../common/include
    ${Boost_INCLUDE_DIRS}
    ${libzmq_INCLUDE}
)

target_link_libraries(cgr-test
    hdtn_reg
    bpcodec
    hdtn_util
    config_lib
    ${pthread_LIB}
    ${libzmq_LIB}
    ${Boost_LIBRARIES}
)
