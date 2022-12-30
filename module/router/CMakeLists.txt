add_library(router_lib
	src/router.cpp
	src/RouterRunner.cpp
)
GENERATE_EXPORT_HEADER(router_lib)
get_target_property(target_type router_lib TYPE)
if (target_type STREQUAL SHARED_LIBRARY)
	set_property(TARGET router_lib PROPERTY CXX_VISIBILITY_PRESET hidden)
	set_property(TARGET router_lib PROPERTY VISIBILITY_INLINES_HIDDEN ON)
endif()
set(MY_PUBLIC_HEADERS
    include/router.h
	include/RouterRunner.h
	${CMAKE_CURRENT_BINARY_DIR}/router_lib_export.h
)
set_target_properties(router_lib PROPERTIES PUBLIC_HEADER "${MY_PUBLIC_HEADERS}") # this needs to be a list, so putting in quotes makes it a ; separated list
target_link_libraries(router_lib
	PUBLIC
		bpcodec
		hdtn_util
		config_lib
		cgr_lib
		Boost::program_options
		log_lib
		telemetry_definitions
)
target_include_directories(router_lib
	PUBLIC
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}> # for GENERATE_EXPORT_HEADER
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../common/include> # for message.hpp
)
install(TARGETS router_lib
	EXPORT router_lib-targets
	DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
add_hdtn_package_export(router_lib RouterLib) #exported target will have the name HDTN::RouterLib and not router_lib.  Also requires install to EXPORT router_lib-targets




add_executable(hdtn-router
    src/main.cpp
)

install(TARGETS hdtn-router DESTINATION ${CMAKE_INSTALL_BINDIR})
target_link_libraries(hdtn-router router_lib)

