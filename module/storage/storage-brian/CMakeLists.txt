cmake_minimum_required(VERSION 3.0)
project(storage-brian-speedtest)
set(CMAKE_CXX_STANDARD 11)
find_package(Boost 1.67.0 REQUIRED system filesystem timer thread regex filesystem program_options iostreams)

add_compile_definitions(USE_BRIAN_STORAGE)

add_executable(storage-brian-speedtest
    src/MemoryManagerTreeArray.cpp
    src/BundleStorageManagerMT.cpp
    src/CircularIndexBufferSingleProducerSingleConsumer.cpp
    ../../../common/util/src/SignalHandler.cpp
    src/StorageConfig.cpp
    ../../../common/util/src/JsonSerializable.cpp
    src/ZmqStorageInterface.cpp #not needed but for compile verification
    src/main.cpp
)
target_link_libraries(storage-brian-speedtest hdtn_reg bpcodec ${Boost_LIBRARIES})
#target_link_directories(storage-brian-speedtest PUBLIC lib)
target_include_directories(storage-brian-speedtest PUBLIC include lib/include ../../../common/bpcodec/include ../../../common/util/include ../../../common/regsvr/lib/include ../../../common/include ../include ${Boost_INCLUDE_DIRS})
if (NOT WIN32)
	target_link_libraries(storage-brian-speedtest pthread)
endif()
