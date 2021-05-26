cmake_minimum_required(VERSION 3.0)
project(unit-tests)
set(CMAKE_CXX_STANDARD 11)

add_executable(unit-tests
    src/test_main.cpp
    ../../common/tcpcl/test/TestTcpcl.cpp
    ../../common/util/test/TestSdnv.cpp
    ../../common/util/test/TestCircularIndexBuffer.cpp
    ../../common/regsvr/lib/test/TestHdtnEntries.cpp
    ../../module/storage/unit_tests/StorageConfigTests.cpp
    ../../module/storage/unit_tests/MemoryManagerTreeTests.cpp
    ../../module/storage/unit_tests/MemoryManagerTreeArrayTests.cpp
    ../../module/storage/unit_tests/BundleStorageManagerMtTests.cpp
    ../../module/storage/unit_tests/BundleStorageManagerMtAsFifoTests.cpp
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
	storage_lib
        ingress_async_lib
	${blosc_LIB}
	bpcodec
	${Boost_LIBRARIES}
)
