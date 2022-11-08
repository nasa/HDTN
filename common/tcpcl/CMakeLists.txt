add_library(tcpcl_lib
    src/Tcpcl.cpp
	src/TcpclV4.cpp
	src/TcpclBundleSink.cpp
	src/TcpclBundleSource.cpp
	src/TcpclV3BidirectionalLink.cpp
	src/TcpclV4BundleSource.cpp
	src/TcpclV4BundleSink.cpp
	src/TcpclV4BidirectionalLink.cpp
)
target_compile_options(tcpcl_lib PRIVATE ${NON_WINDOWS_HARDWARE_ACCELERATION_FLAGS})
GENERATE_EXPORT_HEADER(tcpcl_lib)
get_target_property(target_type tcpcl_lib TYPE)
if (target_type STREQUAL SHARED_LIBRARY)
	set_property(TARGET tcpcl_lib PROPERTY CXX_VISIBILITY_PRESET hidden)
	set_property(TARGET tcpcl_lib PROPERTY VISIBILITY_INLINES_HIDDEN ON)
endif()
set(MY_PUBLIC_HEADERS
    include/BidirectionalLink.h
	include/Tcpcl.h
	include/TcpclBundleSink.h
	include/TcpclBundleSource.h
	include/TcpclV3BidirectionalLink.h
	include/TcpclV4.h
	include/TcpclV4BidirectionalLink.h
	include/TcpclV4BundleSink.h
	include/TcpclV4BundleSource.h
	${CMAKE_CURRENT_BINARY_DIR}/tcpcl_lib_export.h
)
set_target_properties(tcpcl_lib PROPERTIES PUBLIC_HEADER "${MY_PUBLIC_HEADERS}") # this needs to be a list, so putting in quotes makes it a ; separated list
target_link_libraries(tcpcl_lib
	PUBLIC
		hdtn_util
		log_lib
)
target_include_directories(tcpcl_lib
	PUBLIC
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}> # for GENERATE_EXPORT_HEADER
)
install(TARGETS tcpcl_lib
	EXPORT tcpcl_lib-targets
	DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
add_hdtn_package_export(tcpcl_lib TcpclLib) #exported target will have the name HDTN::TcpclLib and not tcpcl_lib.  Also requires install to EXPORT tcpcl_lib-targets
