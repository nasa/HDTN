add_library(telemetry_definitions
	src/Telemetry.cpp
)
GENERATE_EXPORT_HEADER(telemetry_definitions)
get_target_property(target_type telemetry_definitions TYPE)
if (target_type STREQUAL SHARED_LIBRARY)
	set_property(TARGET telemetry_definitions PROPERTY CXX_VISIBILITY_PRESET hidden)
	set_property(TARGET telemetry_definitions PROPERTY VISIBILITY_INLINES_HIDDEN ON)
endif()
set(MY_PUBLIC_HEADERS
    include/Telemetry.h
	${CMAKE_CURRENT_BINARY_DIR}/telemetry_definitions_export.h
)
set_target_properties(telemetry_definitions PROPERTIES PUBLIC_HEADER "${MY_PUBLIC_HEADERS}") # this needs to be a list, so putting in quotes makes it a ; separated list
target_link_libraries(telemetry_definitions
	PUBLIC
		log_lib
		Boost::boost #boost headers
)
target_include_directories(telemetry_definitions
	PUBLIC
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}> # for GENERATE_EXPORT_HEADER
)
install(TARGETS telemetry_definitions
	EXPORT telemetry_definitions-targets
	DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
add_hdtn_package_export(telemetry_definitions TelemetryDefinitions) #exported target will have the name HDTN::TelemetryDefinitions and not telemetry_definitions.  Also requires install to EXPORT telemetry_definitions-targets
