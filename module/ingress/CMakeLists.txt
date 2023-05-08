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
	${CMAKE_CURRENT_BINARY_DIR}/ingress_async_lib_export.h
)
set_target_properties(ingress_async_lib PROPERTIES PUBLIC_HEADER "${MY_PUBLIC_HEADERS}") # this needs to be a list, so putting in quotes makes it a ; separated list
target_link_libraries(ingress_async_lib
	PUBLIC
		hdtn_util
		log_lib
		induct_manager_lib
	        $<$<BOOL:${ENABLE_BPSEC}>:bpsec>
		bpcodec
		bpsec
		telemetry_definitions
		Boost::program_options
)
target_include_directories(ingress_async_lib
	PUBLIC
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}> # for GENERATE_EXPORT_HEADER
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../common/include> # for message.hpp
)
install(TARGETS ingress_async_lib
	EXPORT ingress_async_lib-targets
	DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
add_hdtn_package_export(ingress_async_lib IngressLib) #exported target will have the name HDTN::IngressLib and not ingress_async_lib.  Also requires install to EXPORT ingress_async_lib-targets


add_executable(hdtn-ingress 
    src/ingress.cpp
)
install(TARGETS hdtn-ingress DESTINATION ${CMAKE_INSTALL_BINDIR})
target_link_libraries(hdtn-ingress ingress_async_lib)
