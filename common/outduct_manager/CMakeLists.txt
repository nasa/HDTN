add_library(outduct_manager_lib
	src/Outduct.cpp
	src/TcpclOutduct.cpp
	src/TcpclV4Outduct.cpp
	src/StcpOutduct.cpp
    src/UdpOutduct.cpp
	src/LtpOutduct.cpp
	src/LtpOverUdpOutduct.cpp
	src/OutductManager.cpp
)
GENERATE_EXPORT_HEADER(outduct_manager_lib)
get_target_property(target_type outduct_manager_lib TYPE)
if (target_type STREQUAL SHARED_LIBRARY)
	set_property(TARGET outduct_manager_lib PROPERTY CXX_VISIBILITY_PRESET hidden)
	set_property(TARGET outduct_manager_lib PROPERTY VISIBILITY_INLINES_HIDDEN ON)
endif()
set(MY_PUBLIC_HEADERS
    include/LtpOverUdpOutduct.h
	include/LtpOutduct.h
	include/Outduct.h
	include/OutductManager.h
	include/StcpOutduct.h
	include/TcpclOutduct.h
	include/TcpclV4Outduct.h
	include/UdpOutduct.h
	${CMAKE_CURRENT_BINARY_DIR}/outduct_manager_lib_export.h
)
set_target_properties(outduct_manager_lib PROPERTIES PUBLIC_HEADER "${MY_PUBLIC_HEADERS}") # this needs to be a list, so putting in quotes makes it a ; separated list
target_link_libraries(outduct_manager_lib
	PUBLIC
		hdtn_util
		config_lib
		tcpcl_lib 
		stcp_lib 
		udp_lib 
		ltp_lib
		bpcodec
		log_lib
)
target_include_directories(outduct_manager_lib
	PUBLIC
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}> # for GENERATE_EXPORT_HEADER
)
install(TARGETS outduct_manager_lib
	EXPORT outduct_manager_lib-targets
	DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
add_hdtn_package_export(outduct_manager_lib OutductManagerLib) #exported target will have the name HDTN::OutductManagerLib and not outduct_manager_lib.  Also requires install to EXPORT outduct_manager_lib-targets
