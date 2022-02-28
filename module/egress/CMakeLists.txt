cmake_minimum_required(VERSION 3.0)
project(hdtn-egress)
#set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 11)

add_library(egress_async_lib
	src/EgressAsync.cpp
	src/EgressAsyncRunner.cpp
)
GENERATE_EXPORT_HEADER(egress_async_lib)
install(TARGETS egress_async_lib DESTINATION lib)
install(FILES
	include/EgressAsync.h
	include/EgressAsyncRunner.h
	${PROJECT_BINARY_DIR}/egress_async_lib_export.h
	DESTINATION include
)
target_link_libraries(egress_async_lib
    hdtn_util
    log_lib
    outduct_manager_lib
    ${libzmq_LIB}
    ${pthread_LIB}
    bpcodec
    ${Boost_LIBRARIES}
)

target_include_directories(egress_async_lib PUBLIC
	include
	../../common/include
	${Boost_INCLUDE_DIRS} 
	${libzmq_INCLUDE}
	${PROJECT_BINARY_DIR}
)

add_executable(hdtn-egress-async
	       src/egress.cpp
)
install(TARGETS hdtn-egress-async DESTINATION bin)
target_link_libraries(hdtn-egress-async egress_async_lib)
target_include_directories(hdtn-egress-async PUBLIC include)

