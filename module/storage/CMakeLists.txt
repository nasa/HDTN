cmake_minimum_required(VERSION 3.0)
project(hdtn-storage)
set(CMAKE_CXX_STANDARD 11)
find_package(Boost REQUIRED COMPONENTS system log log_setup)


SET(STORAGE_LIBRARY_NAME storage_lib)
add_executable(hdtn-storage
        src/StorageRunner.cpp
        src/main.cpp
        src/store.cpp
	../../common/logger/src/Logger.cpp
)

target_link_libraries(hdtn-storage hdtn_reg bpcodec hdtn_util ${STORAGE_LIBRARY_NAME} ${libzmq_LIB} ${pthread_LIB} ${Boost_LIBRARIES} log_lib)

target_include_directories(hdtn-storage PUBLIC
	include
	${Boost_INCLUDE_DIRS}
	${libzmq_INCLUDE}
	)

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
target_link_libraries(hdtn-test-schedule hdtn_reg bpcodec ${libzmq_LIB} ${pthread_LIB} ${Boost_LIBRARIES})
target_include_directories(hdtn-test-schedule PUBLIC
	include
	lib/include
	../../common/bpcodec/include
	../../common/regsvr/lib/include
	../../common/include
	${Boost_INCLUDE_DIRS}
	${libzmq_INCLUDE}
)
	
add_executable(hdtn-release-message-sender
	src/test/release_message_sender.cpp
)
target_link_libraries(hdtn-release-message-sender hdtn_reg bpcodec ${libzmq_LIB} ${pthread_LIB} ${Boost_LIBRARIES})
target_include_directories(hdtn-release-message-sender PUBLIC
	include
	lib/include
	../../common/bpcodec/include
	../../common/regsvr/lib/include
	../../common/include
	${Boost_INCLUDE_DIRS}
	${libzmq_INCLUDE}
)

add_executable(hdtn-multiple-release-message-sender
    src/test/ReleaseSender.cpp
    src/test/multiple_release_message_sender.cpp
)
target_link_libraries(hdtn-multiple-release-message-sender hdtn_reg bpcodec ${libzmq_LIB} ${pthread_LIB}  ${Boost_LIBRARIES})
target_include_directories(hdtn-multiple-release-message-sender PUBLIC
    include
    lib/include
    ../../common/bpcodec/include
    ../../common/regsvr/lib/include
    ../../common/include
    ${Boost_INCLUDE_DIRS}
    ${libzmq_INCLUDE}
)

add_library(storage_lib STATIC
        src/MemoryManagerTreeArray.cpp
        src/BundleStorageManagerMT.cpp
        src/StorageConfig.cpp
        src/ZmqStorageInterface.cpp
)

target_link_libraries(storage_lib hdtn_util hdtn_reg bpcodec ${Boost_LIBRARIES} ${pthread_LIB} ${libzmq_LIB})
target_include_directories(storage_lib PUBLIC
        include
        ../../common/include
        ${Boost_INCLUDE_DIRS}
        ${libzmq_INCLUDE}
)

add_executable(storage-speedtest
        src/speedtest.cpp
)
target_link_libraries(storage-speedtest hdtn_reg bpcodec hdtn_util storage_lib ${Boost_LIBRARIES} ${pthread_LIB})
target_include_directories(storage-speedtest PUBLIC ${Boost_INCLUDE_DIRS})

