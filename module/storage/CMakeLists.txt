cmake_minimum_required(VERSION 3.0)
project(hdtn-storage)
set(CMAKE_CXX_STANDARD 11)


add_executable(hdtn-storage
        src/StorageRunner.cpp
        src/main.cpp
)
install(TARGETS hdtn-storage DESTINATION bin)
target_link_libraries(hdtn-storage bpcodec hdtn_util storage_lib log_lib ${libzmq_LIB} ${pthread_LIB} ${Boost_LIBRARIES} log_lib)


target_include_directories(hdtn-storage PUBLIC
	include
	${Boost_INCLUDE_DIRS}
	${libzmq_INCLUDE}
)


	
add_executable(hdtn-release-message-sender
	src/test/release_message_sender.cpp
)
install(TARGETS hdtn-release-message-sender DESTINATION bin)
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
install(TARGETS hdtn-multiple-release-message-sender DESTINATION bin)
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

add_library(storage_lib
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
GENERATE_EXPORT_HEADER(storage_lib)
set_property(TARGET storage_lib PROPERTY CXX_VISIBILITY_PRESET hidden)
set_property(TARGET storage_lib PROPERTY VISIBILITY_INLINES_HIDDEN ON)
install(TARGETS storage_lib DESTINATION lib)
install(FILES
	include/BundleStorageCatalog.h
	include/BundleStorageConfig.h
	include/BundleStorageManagerAsio.h
	include/BundleStorageManagerBase.h
	include/BundleStorageManagerMT.h
	include/CatalogEntry.h
	include/CustodyTimers.h
	include/HashMap16BitFixedSize.h
	include/MemoryManagerTree.h
	include/MemoryManagerTreeArray.h
	include/StorageRunner.h
	include/ZmqStorageInterface.h
	${PROJECT_BINARY_DIR}/storage_lib_export.h
	DESTINATION include
)
target_link_libraries(storage_lib hdtn_util config_lib bpcodec log_lib ${Boost_LIBRARIES} ${pthread_LIB} ${libzmq_LIB})
target_include_directories(storage_lib PUBLIC
        include
        ../../common/include
        ${Boost_INCLUDE_DIRS}
        ${libzmq_INCLUDE}
		${PROJECT_BINARY_DIR}
)

add_executable(storage-speedtest
        src/test/StorageSpeedTestMain.cpp
)
install(TARGETS storage-speedtest DESTINATION bin)
target_link_libraries(storage-speedtest bpcodec hdtn_util storage_lib ${Boost_LIBRARIES} ${pthread_LIB})
target_include_directories(storage-speedtest PUBLIC ${Boost_INCLUDE_DIRS})

