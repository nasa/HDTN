cmake_minimum_required(VERSION 3.0)
project(hdtn_util)
set(CMAKE_CXX_STANDARD 11)



add_library(hdtn_util STATIC
    src/CircularIndexBufferSingleProducerSingleConsumerConfigurable.cpp
    src/Environment.cpp
	src/JsonSerializable.cpp
	src/SignalHandler.cpp
	src/TimestampUtil.cpp
	src/TcpAsyncSender.cpp
	src/Sdnv.cpp
	src/RateManagerAsync.cpp
)
target_link_libraries(hdtn_util ${Boost_LIBRARIES} ${pthread_LIB} ${libzmq_LIB})
target_include_directories(hdtn_util PUBLIC include ../regsvr/lib/include ${Boost_INCLUDE_DIRS} ${libzmq_INCLUDE})


