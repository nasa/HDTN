cmake_minimum_required(VERSION 3.0)
project(stcp_lib)
set(CMAKE_CXX_STANDARD 11)



add_library(stcp_lib STATIC
	src/StcpBundleSink.cpp
	src/StcpBundleSource.cpp
)

target_link_libraries(stcp_lib hdtn_util ${Boost_LIBRARIES} ${pthread_LIB})
target_include_directories(stcp_lib PUBLIC include ${Boost_INCLUDE_DIRS})


