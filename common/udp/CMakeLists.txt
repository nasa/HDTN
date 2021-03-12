cmake_minimum_required(VERSION 3.0)
project(udp_lib)
set(CMAKE_CXX_STANDARD 11)



add_library(udp_lib STATIC
	src/UdpBundleSink.cpp
	src/UdpBundleSource.cpp
)

target_link_libraries(udp_lib hdtn_util ${Boost_LIBRARIES} ${pthread_LIB})
target_include_directories(udp_lib PUBLIC include ${Boost_INCLUDE_DIRS})


