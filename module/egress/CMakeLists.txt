cmake_minimum_required(VERSION 3.0)
project(hdtn-egress)
#set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 11)

add_library(egress_async_lib STATIC
	src/EgressAsync.cpp
	src/EgressAsyncRunner.cpp
)

target_link_libraries(egress_async_lib
    hdtn_util
    log_lib
    outduct_manager_lib
    ${libzmq_LIB}
    ${pthread_LIB}
    ${blosc_LIB}
    bpcodec
    ${Boost_LIBRARIES}
)

target_include_directories(egress_async_lib PUBLIC
	include
	../../common/pool/include
	../../common/include
	${Boost_INCLUDE_DIRS} 
	${libzmq_INCLUDE}
)

add_executable(hdtn-egress-async
	       src/egress.cpp
)

target_link_libraries(hdtn-egress-async egress_async_lib)
target_include_directories(hdtn-egress-async PUBLIC 
	../../common/pool/include
	../../common/include
	${Boost_INCLUDE_DIRS} 
	${libzmq_INCLUDE}
)

