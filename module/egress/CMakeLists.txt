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
	${CMAKE_CURRENT_BINARY_DIR}/egress_async_lib_export.h
)
set_target_properties(egress_async_lib PROPERTIES PUBLIC_HEADER "${MY_PUBLIC_HEADERS}") # this needs to be a list, so putting in quotes makes it a ; separated list
target_link_libraries(egress_async_lib
	PUBLIC
		hdtn_util
		log_lib
		outduct_manager_lib
		bpcodec
		Boost::program_options
)
target_include_directories(egress_async_lib
	PUBLIC
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}> # for GENERATE_EXPORT_HEADER
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../common/include> # for message.hpp
)
install(TARGETS egress_async_lib
	EXPORT egress_async_lib-targets
	DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
add_hdtn_package_export(egress_async_lib EgressLib) #exported target will have the name HDTN::EgressLib and not egress_async_lib.  Also requires install to EXPORT egress_async_lib-targets



add_executable(hdtn-egress-async
	       src/egress.cpp
)
install(TARGETS hdtn-egress-async DESTINATION ${CMAKE_INSTALL_BINDIR})
target_link_libraries(hdtn-egress-async egress_async_lib)
