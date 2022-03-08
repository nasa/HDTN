cmake_minimum_required(VERSION 3.0)
project(hdtn-egress)
set(CMAKE_CXX_STANDARD 11)

add_library(egress_async_lib
	src/EgressAsync.cpp
	src/EgressAsyncRunner.cpp
)
GENERATE_EXPORT_HEADER(egress_async_lib)
get_target_property(target_type egress_async_lib TYPE)
if (target_type STREQUAL SHARED_LIBRARY)
	set_property(TARGET egress_async_lib PROPERTY CXX_VISIBILITY_PRESET hidden)
	set_property(TARGET egress_async_lib PROPERTY VISIBILITY_INLINES_HIDDEN ON)
endif()
set(MY_PUBLIC_HEADERS
    include/EgressAsync.h
	include/EgressAsyncRunner.h
	${PROJECT_BINARY_DIR}/egress_async_lib_export.h
)
set_target_properties(egress_async_lib PROPERTIES PUBLIC_HEADER "${MY_PUBLIC_HEADERS}") # this needs to be a list, so putting in quotes makes it a ; separated list
install(TARGETS egress_async_lib
	DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
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
install(TARGETS hdtn-egress-async DESTINATION ${CMAKE_INSTALL_BINDIR})
target_link_libraries(hdtn-egress-async egress_async_lib)
target_include_directories(hdtn-egress-async PUBLIC include)

