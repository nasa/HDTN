cmake_minimum_required(VERSION 3.0)
project(unit-tests)
#set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 11)

find_package(Boost 1.67.0 REQUIRED system filesystem timer thread regex filesystem program_options iostreams unit_test_framework)

if (WIN32)
	#add_compile_definitions(ZMQ_STATIC)
	set(libzmq_INCLUDE "C:/libzmq_msvc_x64/include" CACHE PATH "Folder to Libzmq headers")
	set(libzmq_LIB "C:/libzmq_msvc_x64/lib/libzmq-v141-mt-4_3_4.lib" CACHE FILEPATH "Libzmq library")
	set(pthread_LIB "")
	set(blosc_LIB "")
	SET(LINUX_ONLY_UNIT_TEST_SRCS "")
else()
	set(libzmq_INCLUDE "")
	set(libzmq_LIB zmq)
	set(pthread_LIB pthread)
	set(blosc_LIB blosc)
	SET(LINUX_ONLY_UNIT_TEST_SRCS
		src/egress_ut.cpp
		src/ingress_ut.cpp
		src/store_ut.cpp
		src/cache_ut.cpp
		../../module/egress/src/internal.cpp
		../../module/egress/src/stcp.cpp
		../../module/egress/src/udp.cpp
		../../module/ingress/src/receive.cpp
		../../common/tcpcl/src/TcpclBundleSink.cpp
		../../common/tcpcl/src/TcpclBundleSource.cpp
		../../common/stcp/src/StcpBundleSink.cpp
		../../common/stcp/src/StcpBundleSource.cpp
		../../module/storage/src/store.cpp
		../../module/storage/src/store-worker.cpp
		../../module/storage/src/cache/cache-basic.cpp
	)
endif()



add_executable(unit-tests
    src/test_main.cpp
    ../../common/tcpcl/test/TestTcpcl.cpp
    ../../common/tcpcl/test/TestSdnv.cpp
    ../../common/regsvr/lib/test/TestHdtnEntries.cpp
    ../../common/tcpcl/src/Tcpcl.cpp
    ../../common/tcpcl/src/sdnv.c
    ../../common/util/src/CircularIndexBufferSingleProducerSingleConsumerConfigurable.cpp
	../../module/storage/storage-brian/unit_tests/StorageConfigTests.cpp
	../../module/storage/storage-brian/unit_tests/MemoryManagerTreeTests.cpp
	../../module/storage/storage-brian/unit_tests/MemoryManagerTreeArrayTests.cpp
	../../module/storage/storage-brian/unit_tests/BundleStorageManagerMtTests.cpp
	../../module/storage/storage-brian/unit_tests/BundleStorageManagerMtAsFifoTests.cpp
	../../module/storage/storage-brian/src/BundleStorageManagerMT.cpp
	../../module/storage/storage-brian/src/CircularIndexBufferSingleProducerSingleConsumer.cpp
	../../module/storage/storage-brian/src/MemoryManagerTree.cpp
	../../module/storage/storage-brian/src/MemoryManagerTreeArray.cpp
	../../module/storage/storage-brian/src/StorageConfig.cpp
    ../../common/util/src/SignalHandler.cpp
    ../../common/util/src/Environment.cpp
    ${LINUX_ONLY_UNIT_TEST_SRCS}
    

)

target_include_directories(unit-tests PUBLIC include
    ../../module/egress/include
    ../../module/ingress/include
    ../../module/storage/include
    ../../common/include
    ../../common/pool/include
    ../../common/logging/include
    ../../common/regsvr/lib/include
    ../../common/bpcodec/include
    ../../common/tcpcl/include
    ../../common/stcp/include
	../../common/util/include
	../../module/storage/storage-brian/include
    ${Boost_INCLUDE_DIRS}
	${libzmq_INCLUDE}
)


#target_link_libraries(boost-unit-tests hdtn_reg bpcodec ${Boost_LIBRARIES})
target_link_libraries(unit-tests ${libzmq_LIB} ${pthread_LIB} hdtn_reg ${blosc_LIB} bpcodec ${Boost_LIBRARIES})


