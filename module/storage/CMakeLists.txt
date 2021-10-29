cmake_minimum_required(VERSION 3.0)
project(hdtn-storage)
set(CMAKE_CXX_STANDARD 11)


add_executable(hdtn-storage
        src/StorageRunner.cpp
        src/main.cpp
)

target_link_libraries(hdtn-storage bpcodec hdtn_util storage_lib log_lib ${libzmq_LIB} ${pthread_LIB} ${Boost_LIBRARIES} log_lib)


target_include_directories(hdtn-storage PUBLIC
	include
	${Boost_INCLUDE_DIRS}
	${libzmq_INCLUDE}
)


	
add_executable(hdtn-release-message-sender
	src/test/release_message_sender.cpp
)
target_link_libraries(hdtn-release-message-sender config_lib bpcodec ${libzmq_LIB} ${pthread_LIB} ${Boost_LIBRARIES})
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
target_link_libraries(hdtn-multiple-release-message-sender config_lib hdtn_util bpcodec ${libzmq_LIB} ${pthread_LIB}  ${Boost_LIBRARIES})
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
		src/BundleStorageManagerAsio.cpp
		src/BundleStorageManagerBase.cpp
		src/HashMap16BitFixedSize.cpp
		src/BundleStorageCatalog.cpp
		src/CustodyTimers.cpp
		src/CatalogEntry.cpp
        src/ZmqStorageInterface.cpp
)

target_link_libraries(storage_lib hdtn_util config_lib bpcodec log_lib ${Boost_LIBRARIES} ${pthread_LIB} ${libzmq_LIB})
target_include_directories(storage_lib PUBLIC
        include
        ../../common/include
        ${Boost_INCLUDE_DIRS}
        ${libzmq_INCLUDE}
)

add_executable(storage-speedtest
        src/test/StorageSpeedTestMain.cpp
)
target_link_libraries(storage-speedtest bpcodec hdtn_util storage_lib ${Boost_LIBRARIES} ${pthread_LIB})
target_include_directories(storage-speedtest PUBLIC ${Boost_INCLUDE_DIRS})

