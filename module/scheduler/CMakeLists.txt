cmake_minimum_required(VERSION 3.0)
project(hdtn-scheduler)
set(CMAKE_CXX_STANDARD 11)

add_executable(hdtn-scheduler 
    src/scheduler.cpp
    src/main.cpp
)

target_link_libraries(hdtn-scheduler
	bpcodec
	hdtn_util
	config_lib
	${libzmq_LIB}
	${pthread_LIB}
	${Boost_LIBRARIES}
)

target_include_directories(hdtn-scheduler PUBLIC
	include
        lib/include
        ../../common/bpcodec/include
        ../../common/include
	${Boost_INCLUDE_DIRS}
	${libzmq_INCLUDE}
)


