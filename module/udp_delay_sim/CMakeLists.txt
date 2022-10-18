add_library(udp_delay_sim_lib
	src/UdpDelaySim.cpp
	src/UdpDelaySimRunner.cpp
)
GENERATE_EXPORT_HEADER(udp_delay_sim_lib)
get_target_property(target_type udp_delay_sim_lib TYPE)
if (target_type STREQUAL SHARED_LIBRARY)
	set_property(TARGET udp_delay_sim_lib PROPERTY CXX_VISIBILITY_PRESET hidden)
	set_property(TARGET udp_delay_sim_lib PROPERTY VISIBILITY_INLINES_HIDDEN ON)
endif()
set(MY_PUBLIC_HEADERS
    include/UdpDelaySim.h
	include/UdpDelaySimRunner.h
	${CMAKE_CURRENT_BINARY_DIR}/udp_delay_sim_lib_export.h
)
set_target_properties(udp_delay_sim_lib PROPERTIES PUBLIC_HEADER "${MY_PUBLIC_HEADERS}") # this needs to be a list, so putting in quotes makes it a ; separated list
target_link_libraries(udp_delay_sim_lib
	PUBLIC
		hdtn_util
		Boost::program_options
)
target_include_directories(udp_delay_sim_lib
	PUBLIC
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}> # for GENERATE_EXPORT_HEADER
)
install(TARGETS udp_delay_sim_lib
	EXPORT udp_delay_sim_lib-targets
	DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
add_hdtn_package_export(udp_delay_sim_lib UdpDelaySimLib) #exported target will have the name HDTN::UdpDelaySimLib and not udp_delay_sim_lib.  Also requires install to EXPORT udp_delay_sim_lib-targets


add_executable(udp-delay-sim 
    src/UdpDelaySimMain.cpp
)
install(TARGETS udp-delay-sim DESTINATION ${CMAKE_INSTALL_BINDIR})
target_link_libraries(udp-delay-sim udp_delay_sim_lib)
