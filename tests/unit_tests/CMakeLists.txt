cmake_minimum_required(VERSION 3.0)
project(unit-tests)
set(CMAKE_CXX_STANDARD 11)


if (WIN32)
	SET(LINUX_ONLY_UNIT_TEST_SRCS "")
else()
    remove_definitions(-DUSE_BRIAN_STORAGE)
	SET(LINUX_ONLY_UNIT_TEST_SRCS
		src/egress_ut.cpp
		src/ingress_ut.cpp
		src/store_ut.cpp
		src/cache_ut.cpp
		../../module/egress/src/internal.cpp
		../../module/egress/src/stcp.cpp
		../../module/egress/src/udp.cpp
		../../module/storage/src/store.cpp
		../../module/storage/src/store-worker.cpp
		../../module/storage/src/cache/cache-basic.cpp
	)
endif()



add_executable(unit-tests
    src/test_main.cpp
    ../../common/tcpcl/test/TestTcpcl.cpp
	../../common/ltp/test/TestLtp.cpp
	../../common/ltp/test/TestLtpFragmentMap.cpp
	../../common/ltp/test/TestLtpRandomNumberGenerator.cpp
	../../common/ltp/test/TestLtpEngine.cpp
    ../../common/util/test/TestSdnv.cpp
	../../common/util/test/TestCircularIndexBuffer.cpp
    ../../common/regsvr/lib/test/TestHdtnEntries.cpp
	../../module/storage/storage-brian/unit_tests/StorageConfigTests.cpp
	../../module/storage/storage-brian/unit_tests/MemoryManagerTreeTests.cpp
	../../module/storage/storage-brian/unit_tests/MemoryManagerTreeArrayTests.cpp
	../../module/storage/storage-brian/unit_tests/BundleStorageManagerMtTests.cpp
	../../module/storage/storage-brian/unit_tests/BundleStorageManagerMtAsFifoTests.cpp
    ${LINUX_ONLY_UNIT_TEST_SRCS}
    

)

target_include_directories(unit-tests PUBLIC include
    ../../module/egress/include
    ../../module/ingress/include
    ../../module/storage/include
    ../../common/include
    ../../common/pool/include
    ../../common/logging/include
    ${Boost_INCLUDE_DIRS}
	${libzmq_INCLUDE}
)


#target_link_libraries(boost-unit-tests hdtn_reg bpcodec ${Boost_LIBRARIES})
target_link_libraries(unit-tests
	${libzmq_LIB}
	${pthread_LIB}
	hdtn_reg
	hdtn_util
	tcpcl_lib
	stcp_lib
	ltp_lib
	storage_brian_lib
        ingress_async_lib
	${blosc_LIB}
	bpcodec
	${Boost_LIBRARIES}
)


