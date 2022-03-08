cmake_minimum_required(VERSION 3.0)
project(hdtn-ingress)
set(CMAKE_CXX_STANDARD 11)

add_library(ingress_async_lib
	src/receive.cpp
	src/IngressAsyncRunner.cpp
)
GENERATE_EXPORT_HEADER(ingress_async_lib)
get_target_property(target_type ingress_async_lib TYPE)
if (target_type STREQUAL SHARED_LIBRARY)
	set_property(TARGET ingress_async_lib PROPERTY CXX_VISIBILITY_PRESET hidden)
	set_property(TARGET ingress_async_lib PROPERTY VISIBILITY_INLINES_HIDDEN ON)
endif()
set(MY_PUBLIC_HEADERS
    include/ingress.h
	include/IngressAsyncRunner.h
	${PROJECT_BINARY_DIR}/ingress_async_lib_export.h
)
set_target_properties(ingress_async_lib PROPERTIES PUBLIC_HEADER "${MY_PUBLIC_HEADERS}") # this needs to be a list, so putting in quotes makes it a ; separated list
install(TARGETS ingress_async_lib
	DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
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
install(TARGETS hdtn-ingress DESTINATION ${CMAKE_INSTALL_BINDIR})
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

