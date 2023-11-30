add_library(bp_over_encap_lib
	src/BpOverEncapBundleSink.cpp
	src/BpOverEncapBundleSource.cpp
)
GENERATE_EXPORT_HEADER(bp_over_encap_lib)
get_target_property(target_type bp_over_encap_lib TYPE)
if (target_type STREQUAL SHARED_LIBRARY)
	set_property(TARGET bp_over_encap_lib PROPERTY CXX_VISIBILITY_PRESET hidden)
	set_property(TARGET bp_over_encap_lib PROPERTY VISIBILITY_INLINES_HIDDEN ON)
endif()
set(MY_PUBLIC_HEADERS
    include/BpOverEncapBundleSink.h
	include/BpOverEncapBundleSource.h
	${CMAKE_CURRENT_BINARY_DIR}/bp_over_encap_lib_export.h
)
set_target_properties(bp_over_encap_lib PROPERTIES PUBLIC_HEADER "${MY_PUBLIC_HEADERS}") # this needs to be a list, so putting in quotes makes it a ; separated list
target_link_libraries(bp_over_encap_lib
	PUBLIC
		hdtn_util
		telemetry_definitions
		log_lib
)
target_include_directories(bp_over_encap_lib
	PUBLIC
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}> # for GENERATE_EXPORT_HEADER
)
install(TARGETS bp_over_encap_lib
	EXPORT bp_over_encap_lib-targets
	DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
add_hdtn_package_export(bp_over_encap_lib BpOverEncapLib) #exported target will have the name HDTN::BpOverEncapLib and not bp_over_encap_lib.  Also requires install to EXPORT bp_over_encap_lib-targets
