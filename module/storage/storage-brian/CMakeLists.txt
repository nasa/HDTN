cmake_minimum_required(VERSION 3.0)
project(storage-brian-speedtest)
set(CMAKE_CXX_STANDARD 11)
find_package(Boost 1.40.0 REQUIRED system filesystem timer thread )

add_compile_definitions(USE_BRIAN_STORAGE)

add_executable(storage-brian-speedtest
    src/MemoryManagerTreeArray.cpp
    src/BundleStorageManagerMT.cpp
    src/CircularIndexBufferSingleProducerSingleConsumer.cpp
    src/SignalHandler.cpp
    src/ZmqStorageInterface.cpp #not needed but for compile verification
    src/main.cpp
)
target_link_libraries(storage-brian-speedtest pthread zmq hdtn_reg bpcodec boost_system boost_filesystem boost_timer boost_thread )
target_link_directories(storage-brian-speedtest PUBLIC lib)
target_include_directories(storage-brian-speedtest PUBLIC include lib/include ../../../common/bpcodec/include ../../../common/regsvr/lib/include ../../../common/include ../include)

