add_library(udp_lib
	src/UdpBundleSink.cpp
	src/UdpBundleSource.cpp
)
GENERATE_EXPORT_HEADER(udp_lib)
get_target_property(target_type udp_lib TYPE)
if (target_type STREQUAL SHARED_LIBRARY)
	set_property(TARGET udp_lib PROPERTY CXX_VISIBILITY_PRESET hidden)
	set_property(TARGET udp_lib PROPERTY VISIBILITY_INLINES_HIDDEN ON)
endif()
set(MY_PUBLIC_HEADERS
    include/UdpBundleSink.h
	include/UdpBundleSource.h
	${CMAKE_CURRENT_BINARY_DIR}/udp_lib_export.h
)
set_target_properties(udp_lib PROPERTIES PUBLIC_HEADER "${MY_PUBLIC_HEADERS}") # this needs to be a list, so putting in quotes makes it a ; separated list
target_link_libraries(udp_lib
	PUBLIC
		hdtn_util
		telemetry_definitions
		log_lib
)
target_include_directories(udp_lib
	PUBLIC
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}> # for GENERATE_EXPORT_HEADER
)
install(TARGETS udp_lib
	EXPORT udp_lib-targets
	DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
add_hdtn_package_export(udp_lib UdpLib) #exported target will have the name HDTN::UdpLib and not udp_lib.  Also requires install to EXPORT udp_lib-targets
