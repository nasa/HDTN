cmake_minimum_required(VERSION 3.0)
project(hdtn-storage-brian)
set(CMAKE_CXX_STANDARD 11)
find_package(Boost 1.40.0 REQUIRED system filesystem timer thread )

add_executable(hdtn-storage-brian
    src/MemoryManagerTreeArray.cpp
    src/BundleStorageManagerMT.cpp
    src/CircularIndexBufferSingleProducerSingleConsumer.cpp
    src/SignalHandler.cpp
    src/main.cpp
)
target_link_libraries(hdtn-storage-brian pthread zmq hdtn_reg bpcodec boost_system boost_filesystem boost_timer boost_thread )
target_link_directories(hdtn-storage-brian PUBLIC lib)
target_include_directories(hdtn-storage-brian PUBLIC include lib/include ../../../common/bpcodec/include ../../../common/regsvr/lib/include ../../../common/include)

