cmake_minimum_required(VERSION 3.0)
project(storage-brian)
set(CMAKE_CXX_STANDARD 11)


	

add_library(storage_brian_lib STATIC
	src/MemoryManagerTreeArray.cpp
	src/BundleStorageManagerMT.cpp
	src/StorageConfig.cpp
	src/ZmqStorageInterface.cpp
)

target_link_libraries(storage_brian_lib hdtn_util hdtn_reg bpcodec ${Boost_LIBRARIES} ${pthread_LIB} ${libzmq_LIB})
target_include_directories(storage_brian_lib PUBLIC
	include
	../../../common/include
	../include
    ../../../common/logger/include
	${Boost_INCLUDE_DIRS}
	${libzmq_INCLUDE}
)




add_executable(storage-brian-speedtest
	src/main.cpp
)
target_link_libraries(storage-brian-speedtest hdtn_reg bpcodec hdtn_util storage_brian_lib ${Boost_LIBRARIES} ${pthread_LIB})
target_include_directories(storage-brian-speedtest PUBLIC ${Boost_INCLUDE_DIRS})
