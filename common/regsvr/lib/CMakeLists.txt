cmake_minimum_required(VERSION 3.0)
project(hdtn_reg)


message("CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
message("CMAKE_CONFIGURATION_TYPES=${CMAKE_CONFIGURATION_TYPES}")


add_library(hdtn_reg STATIC
    src/reg.cpp
)


target_link_libraries(hdtn_reg ${libzmq_LIB} ${pthread_LIB} bpcodec hdtn_util ${Boost_LIBRARIES})
target_include_directories(hdtn_reg PUBLIC 
	include
	${Boost_INCLUDE_DIRS}
	${libzmq_INCLUDE}
)


add_executable(hdtn_regtest
    src/reg.cpp
    src/main.cpp
)

target_link_libraries(hdtn_regtest hdtn_util ${libzmq_LIB} ${Boost_LIBRARIES} ${pthread_LIB})
target_include_directories(hdtn_regtest PUBLIC
	include
	${libzmq_INCLUDE}
	${Boost_INCLUDE_DIRS}
)

