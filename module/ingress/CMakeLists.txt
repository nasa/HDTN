cmake_minimum_required(VERSION 3.0)
project(hdtn-ingress)
set(CMAKE_CXX_STANDARD 11)

add_library(ingress_async_lib
	src/receive.cpp
	src/IngressAsyncRunner.cpp
)
GENERATE_EXPORT_HEADER(ingress_async_lib)
target_link_libraries(ingress_async_lib 
	hdtn_util 
    log_lib
	induct_manager_lib
	${libzmq_LIB} 
	${pthread_LIB}
	bpcodec 
	${Boost_LIBRARIES}
)
target_include_directories(ingress_async_lib PUBLIC
	include
	../../common/include
	${Boost_INCLUDE_DIRS} 
	${libzmq_INCLUDE}
	${PROJECT_BINARY_DIR}
)

add_executable(hdtn-ingress 
    src/ingress.cpp
)

target_link_libraries(hdtn-ingress
	bpcodec
	hdtn_util
    log_lib
	ingress_async_lib
	${libzmq_LIB}
	${pthread_LIB}
	${Boost_LIBRARIES}
)
target_include_directories(hdtn-ingress PUBLIC include)

