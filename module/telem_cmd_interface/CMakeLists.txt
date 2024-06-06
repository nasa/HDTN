add_library(telem_lib
	$<$<BOOL:${WEB_INTERFACE_USE_BEAST}>:src/BeastWebsocketServer.cpp>
	$<$<BOOL:${WEB_INTERFACE_USE_CIVETWEB}>:src/CivetwebWebsocketServer.cpp>
	src/TelemetryRunner.cpp
	src/Telemetry.cpp
	src/TelemetryConnection.cpp
	src/TelemetryConnectionPoller.cpp
	src/TelemetryLogger.cpp
	src/TelemetryRunnerProgramOptions.cpp
	src/WebsocketServer.cpp
)
GENERATE_EXPORT_HEADER(telem_lib)
get_target_property(target_type telem_lib TYPE)
if (target_type STREQUAL SHARED_LIBRARY)
	set_property(TARGET telem_lib PROPERTY CXX_VISIBILITY_PRESET hidden)
	set_property(TARGET telem_lib PROPERTY VISIBILITY_INLINES_HIDDEN ON)
endif()
set(MY_PUBLIC_HEADERS
	#$<$<BOOL:${WEB_INTERFACE_USE_BEAST}>:include/BeastWebsocketServer.h> #generator does not work here
	include/TelemetryRunner.h
	include/Telemetry.h
	include/TelemetryConnection.h
	include/TelemetryConnectionPoller.h
	include/TelemetryLogger.h
	include/TelemetryRunnerProgramOptions.h
	include/WebsocketServer.h
	${CMAKE_CURRENT_BINARY_DIR}/telem_lib_export.h
)
if(WEB_INTERFACE_USE_BEAST)
	list(APPEND MY_PUBLIC_HEADERS include/BeastWebsocketServer.h)
elseif(WEB_INTERFACE_USE_CIVETWEB)
	list(APPEND MY_PUBLIC_HEADERS include/CivetwebWebsocketServer.h)
endif()

#boost beast creates large object files
if(WEB_INTERFACE_USE_BEAST)
	if (MSVC)
		target_compile_options(telem_lib PRIVATE /bigobj)
	else()
		# target_compile_options(telem_lib PRIVATE -Wa -mbig-obj) #mingw-w64 only
	endif()
endif()

if(WEB_INTERFACE_USE_CIVETWEB)
	add_dependencies(telem_lib civetweb-custom-target)
endif()


set_property(
	SOURCE src/WebsocketServer.cpp
	PROPERTY COMPILE_DEFINITIONS
		$<$<BOOL:${WEB_INTERFACE_USE_BEAST}>:WEB_INTERFACE_USE_BEAST> 
		$<$<BOOL:${WEB_INTERFACE_USE_CIVETWEB}>:WEB_INTERFACE_USE_CIVETWEB>
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
	PRIVATE
		$<$<BOOL:${WEB_INTERFACE_USE_CIVETWEB}>:${CIVETWEB_LIB}>
		$<$<BOOL:${WEB_INTERFACE_USE_CIVETWEB}>:${CIVETWEB_CPP_LIB}> 
)
target_include_directories(telem_lib
	PUBLIC
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> #project does not have a /include directory
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}> # for GENERATE_EXPORT_HEADER
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../common/include> # for HdtnVersion.hpp
	PRIVATE
		$<$<BOOL:${WEB_INTERFACE_USE_CIVETWEB}>:${CIVETWEB_INSTALL_INCLUDE_DIRECTORY}>
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

if(USE_WEB_INTERFACE)
    install(DIRECTORY src/gui DESTINATION ${CMAKE_INSTALL_DATADIR})

	#check if user wants to build new gui
	if(DEFINED ENV{HDTN_GUI_VERSION})
		if("$ENV{HDTN_GUI_VERSION}" STREQUAL "2")
			message("Environmental variable HDTN_GUI_VERSION is set to 2. Will use new GUI v2.")
			add_subdirectory(src/gui_v2)
		else()
			message("Environmental variable HDTN_GUI_VERSION was not set to 2. It was set to $ENV{HDTN_GUI_VERSION}. Using old GUI.")
		endif()
	else()
		message("Environmental variable HDTN_GUI_VERSION was not defined. Using old GUI.")
	endif()

endif()
