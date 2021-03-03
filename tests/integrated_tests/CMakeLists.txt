cmake_minimum_required(VERSION 3.0)
project(integrated-tests)
set(CMAKE_CXX_STANDARD 11)


if (WIN32)
	SET(LINUX_ONLY_INTEGRATED_TEST_SRCS "")
else()
	SET(LINUX_ONLY_INTEGRATED_TEST_SRCS
		../../module/egress/src/internal.cpp
		../../module/egress/src/stcp.cpp
		../../module/egress/src/udp.cpp
		../../module/storage/src/cache/cache-basic.cpp
	)
endif()

if(USE_BRIAN_STORAGE)
	SET(STORAGE_BRIAN_LIBRARY_NAME storage_brian_lib)
else()
	SET(STORAGE_BRIAN_LIBRARY_NAME "")
	SET(LINUX_ONLY_INTEGRATED_TEST_SRCS ${LINUX_ONLY_INTEGRATED_TEST_SRCS} 
	../../module/storage/src/store-worker.cpp)
endif()

add_executable(integrated-tests
	src/it_test_main.cpp
	src/integrated_tests.cpp
	../../common/logging/src/logging.cpp
    ../../common/bpcodec/apps/bpgen/src/BpGenAsync.cpp
    ../../common/bpcodec/apps/bpsink/BpSinkAsync.cpp

	../../module/storage/src/store.cpp

	${LINUX_ONLY_INTEGRATED_TEST_SRCS}
)

target_include_directories(integrated-tests PUBLIC include
	../../module/storage/include
	../../common/include
	../../common/pool/include
	../../common/logging/include
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
	${STORAGE_BRIAN_LIBRARY_NAME}
	hdtn_reg
	hdtn_util
	tcpcl_lib
	stcp_lib
	egress_async_lib
	ingress_async_lib
	${blosc_LIB}
	bpcodec
	${Boost_LIBRARIES}
)




