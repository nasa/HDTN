add_library(induct_manager_lib
	src/Induct.cpp
	src/TcpclInduct.cpp
	src/TcpclV4Induct.cpp
	src/StcpInduct.cpp
    src/UdpInduct.cpp
	src/LtpOverUdpInduct.cpp
	src/InductManager.cpp
)
GENERATE_EXPORT_HEADER(induct_manager_lib)
get_target_property(target_type induct_manager_lib TYPE)
if (target_type STREQUAL SHARED_LIBRARY)
	set_property(TARGET induct_manager_lib PROPERTY CXX_VISIBILITY_PRESET hidden)
	set_property(TARGET induct_manager_lib PROPERTY VISIBILITY_INLINES_HIDDEN ON)
endif()
set(MY_PUBLIC_HEADERS
    include/Induct.h
	include/InductManager.h
	include/LtpOverUdpInduct.h
	include/StcpInduct.h
	include/TcpclInduct.h
	include/TcpclV4Induct.h
	include/UdpInduct.h
	${CMAKE_CURRENT_BINARY_DIR}/induct_manager_lib_export.h
)
set_target_properties(induct_manager_lib PROPERTIES PUBLIC_HEADER "${MY_PUBLIC_HEADERS}") # this needs to be a list, so putting in quotes makes it a ; separated list
target_link_libraries(induct_manager_lib
	PUBLIC
		hdtn_util
		config_lib
		tcpcl_lib 
		stcp_lib 
		udp_lib 
		ltp_lib
)
target_include_directories(induct_manager_lib
	PUBLIC
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}> # for GENERATE_EXPORT_HEADER
)
install(TARGETS induct_manager_lib
	EXPORT induct_manager_lib-targets
	DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
add_hdtn_package_export(induct_manager_lib InductManagerLib) #exported target will have the name HDTN::InductManagerLib and not induct_manager_lib.  Also requires install to EXPORT induct_manager_lib-targets
