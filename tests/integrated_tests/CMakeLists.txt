cmake_minimum_required(VERSION 3.0)
project(integrated-tests)
#set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 11)

find_package(Boost 1.67.0 REQUIRED system filesystem timer thread regex filesystem program_options unit_test_framework iostreams)

# define _WIN32_WINNT to use Windows 10 for boost
if(WIN32)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_definitions(_WIN32_WINNT=0x0A00)
endif()

OPTION(USE_BRIAN_STORAGE "use brian's storage solution" ON)
if(USE_BRIAN_STORAGE)
    add_compile_definitions(USE_BRIAN_STORAGE)
endif()

if (WIN32)
	#add_compile_definitions(ZMQ_STATIC)
	set(libzmq_INCLUDE "C:/libzmq_msvc_x64/include" CACHE PATH "Folder to Libzmq headers")
	set(libzmq_LIB "C:/libzmq_msvc_x64/lib/libzmq-v141-mt-4_3_4.lib" CACHE FILEPATH "Libzmq library")
	set(pthread_LIB "")
	set(blosc_LIB "")
	SET(LINUX_ONLY_INTEGRATED_TEST_SRCS "")
else()
	set(libzmq_INCLUDE "")
	set(libzmq_LIB zmq)
	set(pthread_LIB pthread)
	set(blosc_LIB blosc)
	SET(LINUX_ONLY_INTEGRATED_TEST_SRCS
		../../module/egress/src/internal.cpp
		../../module/egress/src/stcp.cpp
		../../module/egress/src/udp.cpp
		../../module/storage/src/cache/cache-basic.cpp
	)
endif()

if( NOT USE_BRIAN_STORAGE)
	SET(LINUX_ONLY_INTEGRATED_TEST_SRCS ${LINUX_ONLY_INTEGRATED_TEST_SRCS} 
	../../module/storage/src/store-worker.cpp)
endif()

add_executable(integrated-tests
	src/it_test_main.cpp
	src/integrated_tests.cpp
	../../common/logging/src/logging.cpp
    ../../common/bpcodec/apps/bpgen/src/BpGenAsync.cpp
    ../../common/bpcodec/apps/bpsink/BpSinkAsync.cpp
    ../../common/tcpcl/src/Tcpcl.cpp
	../../common/tcpcl/src/TcpclBundleSink.cpp
	../../common/tcpcl/src/TcpclBundleSource.cpp

	../../common/stcp/src/StcpBundleSink.cpp
	../../common/stcp/src/StcpBundleSource.cpp

	../../common/tcpcl/src/sdnv.c
	../../module/egress/src/EgressAsync.cpp
	../../module/egress/src/EgressAsyncTcpcl.cpp
	../../module/egress/src/EgressAsyncStcp.cpp
	../../module/ingress/src/receive.cpp
	../../common/util/src/CircularIndexBufferSingleProducerSingleConsumerConfigurable.cpp
	../../common/util/src/SignalHandler.cpp
	../../module/storage/src/store.cpp

	../../module/storage/storage-brian/src/BundleStorageManagerMT.cpp
	../../module/storage/storage-brian/src/CircularIndexBufferSingleProducerSingleConsumer.cpp
	../../module/storage/storage-brian/src/MemoryManagerTree.cpp
	../../module/storage/storage-brian/src/MemoryManagerTreeArray.cpp
	../../module/storage/storage-brian/src/StorageConfig.cpp
	../../module/storage/storage-brian/src/ZmqStorageInterface.cpp
    ../../common/util/src/JsonSerializable.cpp
    ../../common/util/src/Environment.cpp
    ${LINUX_ONLY_INTEGRATED_TEST_SRCS}
)

target_include_directories(integrated-tests PUBLIC include
	../../module/egress/include
	../../module/ingress/include
	../../module/storage/include
	../../module/storage/storage-brian/include
	../../common/include
	../../common/pool/include
	../../common/logging/include
	../../common/regsvr/lib/include
	../../common/bpcodec/include
    ../../common/bpcodec/apps/bpgen/include
    ../../common/bpcodec/apps/bpsink/include
    ../../common/tcpcl/include
	../../common/stcp/include
	../../common/util/include
	${Boost_INCLUDE_DIRS}
	${libzmq_INCLUDE}
)

target_link_libraries(integrated-tests ${libzmq_LIB} ${pthread_LIB} hdtn_reg ${blosc_LIB} bpcodec ${Boost_LIBRARIES} )




