cmake_minimum_required(VERSION 3.0)
project(outduct_manager_lib)
set(CMAKE_CXX_STANDARD 11)



add_library(outduct_manager_lib
	src/Outduct.cpp
	src/TcpclOutduct.cpp
	src/TcpclV4Outduct.cpp
	src/StcpOutduct.cpp
    src/UdpOutduct.cpp
	src/LtpOverUdpOutduct.cpp
	src/OutductManager.cpp
)
GENERATE_EXPORT_HEADER(outduct_manager_lib)
get_target_property(target_type outduct_manager_lib TYPE)
if (target_type STREQUAL SHARED_LIBRARY)
	set_property(TARGET outduct_manager_lib PROPERTY CXX_VISIBILITY_PRESET hidden)
	set_property(TARGET outduct_manager_lib PROPERTY VISIBILITY_INLINES_HIDDEN ON)
endif()
install(TARGETS outduct_manager_lib DESTINATION lib)
install(FILES
	include/LtpOverUdpOutduct.h
	include/Outduct.h
	include/OutductManager.h
	include/StcpOutduct.h
	include/TcpclOutduct.h
	include/TcpclV4Outduct.h
	include/UdpOutduct.h
	${PROJECT_BINARY_DIR}/outduct_manager_lib_export.h
	DESTINATION include
)
target_link_libraries(outduct_manager_lib 
	hdtn_util
	config_lib
	tcpcl_lib 
	stcp_lib 
	udp_lib 
	ltp_lib
	bpcodec
	${Boost_LIBRARIES}
	${pthread_LIB}
)
target_include_directories(outduct_manager_lib PUBLIC include ${Boost_INCLUDE_DIRS} ${PROJECT_BINARY_DIR})


