cmake_minimum_required(VERSION 3.0)
project(hdtn-storage)
set(CMAKE_CXX_STANDARD 11)
find_package(Boost 1.67.0 REQUIRED system filesystem timer thread regex filesystem program_options iostreams)

if (WIN32)
	#add_compile_definitions(ZMQ_STATIC)
	set(libzmq_INCLUDE "C:/libzmq_msvc_x64/include" CACHE PATH "Folder to Libzmq headers")
	set(libzmq_LIB "C:/libzmq_msvc_x64/lib/libzmq-v141-mt-4_3_4.lib" CACHE FILEPATH "Libzmq library")
endif()



OPTION(USE_BRIAN_STORAGE "use brian's storage solution" ON)
if(USE_BRIAN_STORAGE)
    add_compile_definitions(USE_BRIAN_STORAGE)
    add_executable(hdtn-storage
        src/main.cpp
        src/store.cpp
        #src/store-worker.cpp
        #src/cache/cache-basic.cpp
        storage-brian/src/MemoryManagerTreeArray.cpp
        storage-brian/src/BundleStorageManagerMT.cpp
        storage-brian/src/CircularIndexBufferSingleProducerSingleConsumer.cpp
        ../../common/util/src/SignalHandler.cpp
        storage-brian/src/ZmqStorageInterface.cpp
        ../../common/util/src/JsonSerializable.cpp
        storage-brian/src/StorageConfig.cpp
    )
    target_link_libraries(hdtn-storage hdtn_reg bpcodec ${Boost_LIBRARIES})
    #target_link_directories(hdtn-storage PUBLIC lib)
    target_include_directories(hdtn-storage PUBLIC include lib/include ../../common/bpcodec/include ../../common/regsvr/lib/include ../../common/util/include ../../common/include storage-brian/include ${Boost_INCLUDE_DIRS})
	
else()
    add_executable(hdtn-storage
        src/main.cpp
        src/store.cpp
        src/store-worker.cpp
        src/cache/cache-basic.cpp
        ../../common/util/src/SignalHandler.cpp
    )
    target_link_libraries(hdtn-storage hdtn_reg blosc bpcodec)
    #target_link_directories(hdtn-storage PUBLIC lib)
    target_include_directories(hdtn-storage PUBLIC include lib/include ../../common/bpcodec/include ../../common/regsvr/lib/include ../../common/include ../../common/util/include)
endif()
if (WIN32)
	target_link_libraries(hdtn-storage ${libzmq_LIB})
	target_include_directories(hdtn-storage PUBLIC ${libzmq_INCLUDE})
else()
	target_link_libraries(hdtn-storage pthread zmq)
endif()

if(NOT WIN32)
	add_executable(hdtn-test-ingress
		src/test/ingress.cpp
	)
	target_link_libraries(hdtn-test-ingress pthread zmq hdtn_reg bpcodec)
	#target_link_directories(hdtn-test-ingress PUBLIC lib)
	target_include_directories(hdtn-test-ingress PUBLIC include lib/include ../../common/bpcodec/include ../../common/regsvr/lib/include ../../common/include)

	add_executable(hdtn-test-egress
		src/test/egress.cpp
	)
	target_link_libraries(hdtn-test-egress pthread zmq hdtn_reg bpcodec)
	#target_link_directories(hdtn-test-egress PUBLIC lib)
	target_include_directories(hdtn-test-egress PUBLIC include lib/include ../../common/bpcodec/include ../../common/regsvr/lib/include ../../common/include)

	add_executable(hdtn-test-release
		src/test/release.cpp
	)
	target_link_libraries(hdtn-test-release pthread zmq hdtn_reg bpcodec)
	#target_link_directories(hdtn-test-release PUBLIC lib)
	target_include_directories(hdtn-test-release PUBLIC include lib/include ../../common/bpcodec/include ../../common/regsvr/lib/include ../../common/include)


	add_executable(hdtn-test-bundle
		src/test/bundle_test.cpp
	)
	target_link_libraries(hdtn-test-bundle pthread zmq hdtn_reg bpcodec ${Boost_LIBRARIES})
	#target_link_directories(hdtn-test-bundle PUBLIC lib)
	target_include_directories(hdtn-test-bundle PUBLIC include lib/include ../../common/bpcodec/include ../../common/regsvr/lib/include ../../common/include ${Boost_INCLUDE_DIRS})


endif()

add_executable(hdtn-test-schedule
	src/test/schedule.cpp
)
target_link_libraries(hdtn-test-schedule hdtn_reg bpcodec ${Boost_LIBRARIES}) 
#target_link_directories(hdtn-test-schedule PUBLIC lib)
target_include_directories(hdtn-test-schedule PUBLIC include lib/include ../../common/bpcodec/include ../../common/regsvr/lib/include ../../common/include ${Boost_INCLUDE_DIRS})
if (WIN32)
	target_link_libraries(hdtn-test-schedule ${libzmq_LIB})
	target_include_directories(hdtn-test-schedule PUBLIC ${libzmq_INCLUDE})
else()
	target_link_libraries(hdtn-test-schedule pthread zmq)
endif()
	
add_executable(hdtn-release-message-sender
	src/test/release_message_sender.cpp
)
target_link_libraries(hdtn-release-message-sender hdtn_reg bpcodec ${Boost_LIBRARIES}) 
target_include_directories(hdtn-release-message-sender PUBLIC include lib/include ../../common/bpcodec/include ../../common/regsvr/lib/include ../../common/include ${Boost_INCLUDE_DIRS})
if (WIN32)
	target_link_libraries(hdtn-release-message-sender ${libzmq_LIB})
	target_include_directories(hdtn-release-message-sender PUBLIC ${libzmq_INCLUDE})
else()
	target_link_libraries(hdtn-release-message-sender pthread zmq)
endif()