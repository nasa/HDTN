add_library(telem_lib
	$<$<BOOL:${USE_WEB_INTERFACE}>:src/BeastWebsocketServer.cpp>
	src/TelemetryRunner.cpp
	src/Telemetry.cpp
	src/TelemetryConnection.cpp
	src/TelemetryConnectionPoller.cpp
	src/TelemetryLogger.cpp
	src/TelemetryRunnerProgramOptions.cpp
)
GENERATE_EXPORT_HEADER(telem_lib)
get_target_property(target_type telem_lib TYPE)
if (target_type STREQUAL SHARED_LIBRARY)
	set_property(TARGET telem_lib PROPERTY CXX_VISIBILITY_PRESET hidden)
	set_property(TARGET telem_lib PROPERTY VISIBILITY_INLINES_HIDDEN ON)
endif()
set(MY_PUBLIC_HEADERS
	#$<$<BOOL:${USE_WEB_INTERFACE}>:include/BeastWebsocketServer.h> #generator does not work here
	include/TelemetryRunner.h
	include/Telemetry.h
	include/TelemetryConnection.h
	include/TelemetryConnectionPoller.h
	include/TelemetryLogger.h
	include/TelemetryRunnerProgramOptions.h
	${CMAKE_CURRENT_BINARY_DIR}/telem_lib_export.h
)
if(USE_WEB_INTERFACE)
	list(APPEND MY_PUBLIC_HEADERS include/BeastWebsocketServer.h)
endif()

#boost beast creates large object files
if(USE_WEB_INTERFACE)
	if (MSVC)
		target_compile_options(telem_lib PRIVATE /bigobj)
	else()
		target_compile_options(telem_lib PRIVATE -Wa -mbig-obj)
	endif()
endif()

set_property(
	SOURCE src/TelemetryRunner.cpp
	PROPERTY COMPILE_DEFINITIONS
		$<$<BOOL:${USE_WEB_INTERFACE}>:USE_WEB_INTERFACE> 
)
set_property(
	SOURCE src/TelemetryRunnerProgramOptions.cpp
	PROPERTY COMPILE_DEFINITIONS
		$<$<BOOL:${USE_WEB_INTERFACE}>:USE_WEB_INTERFACE> 
)
set_target_properties(telem_lib PROPERTIES PUBLIC_HEADER "${MY_PUBLIC_HEADERS}") # this needs to be a list, so putting in quotes makes it a ; separated list
target_link_libraries(telem_lib
	PUBLIC
		hdtn_util
		log_lib
		bpcodec
		config_lib
		stats_lib
		telemetry_definitions
		Boost::program_options
        ${CMAKE_DL_LIBS}
)
target_include_directories(telem_lib
	PUBLIC
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> #project does not have a /include directory
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}> # for GENERATE_EXPORT_HEADER
)
install(TARGETS telem_lib
	EXPORT telem_lib-targets
	DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

add_hdtn_package_export(telem_lib HdtnTelemLib) #exported target will have the name HDTN::HdtnTelemLib and not telem_lib.  Also requires install to EXPORT telem_lib-targets

add_executable(telem_cmd_interface
    src/main.cpp
)

install(TARGETS telem_cmd_interface DESTINATION ${CMAKE_INSTALL_BINDIR})
target_link_libraries(telem_cmd_interface telem_lib)
