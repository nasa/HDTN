cmake_minimum_required(VERSION 3.0)
project(hdtn-router)
set(CMAKE_CXX_STANDARD 11)

add_executable(hdtn-router 
    src/router.cpp
    src/main.cpp
    src/cgrServer.cpp
)
install(TARGETS hdtn-router DESTINATION ${CMAKE_INSTALL_BINDIR})
target_link_libraries(hdtn-router
	bpcodec
	hdtn_util
	config_lib
	${libzmq_LIB}
	${pthread_LIB}
	${Boost_LIBRARIES}
)

target_include_directories(hdtn-router PUBLIC
	include
        lib/include
        ../../common/bpcodec/include
        ../../common/include
	${Boost_INCLUDE_DIRS}
	${libzmq_INCLUDE}
)


