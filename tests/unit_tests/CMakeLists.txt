cmake_minimum_required(VERSION 3.0)
project(unit-tests)
set(CMAKE_CXX_STANDARD 11)

add_executable(unit-tests
    src/test_main.cpp
    ../../common/tcpcl/test/TestTcpcl.cpp
	../../common/ltp/test/TestLtp.cpp
	../../common/ltp/test/TestLtpFragmentSet.cpp
	../../common/ltp/test/TestLtpRandomNumberGenerator.cpp
	../../common/ltp/test/TestLtpEngine.cpp
	../../common/ltp/test/TestLtpUdpEngine.cpp
	../../common/ltp/test/TestLtpTimerManager.cpp
    ../../common/util/test/TestSdnv.cpp
	../../common/util/test/TestCircularIndexBuffer.cpp
	../../common/util/test/TestRateManagerAsync.cpp
	../../common/util/test/TestTimestampUtil.cpp
	../../common/util/test/TestUri.cpp
	../../common/bpcodec/test/TestBpv6AdministrativeRecords
	../../common/config/test/TestInductsConfig.cpp
	../../common/config/test/TestOutductsConfig.cpp
	../../common/config/test/TestStorageConfig.cpp
	../../common/config/test/TestHdtnConfig.cpp
    ../../common/regsvr/lib/test/TestHdtnEntries.cpp
    ../../module/storage/unit_tests/MemoryManagerTreeTests.cpp
    ../../module/storage/unit_tests/MemoryManagerTreeArrayTests.cpp
    ../../module/storage/unit_tests/BundleStorageManagerMtTests.cpp
    ../../module/storage/unit_tests/BundleStorageManagerMtAsFifoTests.cpp
    ../../common/logger/src/Logger.cpp
)

target_include_directories(unit-tests PUBLIC include
    ../../module/egress/include
    ../../module/ingress/include
    ../../module/storage/include
    ../../common/include
    ../../common/pool/include
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
	storage_lib
	config_lib
    ingress_async_lib
	bpcodec
	${Boost_LIBRARIES}
)
