cmake_minimum_required(VERSION 3.0)
project(tcpcl_lib)
set(CMAKE_CXX_STANDARD 11)



add_library(tcpcl_lib STATIC
    src/Tcpcl.cpp
	src/TcpclBundleSink.cpp
	src/TcpclBundleSource.cpp
)

target_link_libraries(tcpcl_lib hdtn_util ${Boost_LIBRARIES} ${pthread_LIB})
target_include_directories(tcpcl_lib PUBLIC include ${Boost_INCLUDE_DIRS})


